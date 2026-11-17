/*
 * Copyright (c) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2020 Lucio Tarantino <https://github.com/dianlight>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ads111x.c
 *
 * ESP-IDF driver for ADS1113/ADS1114/ADS1115, ADS1013/ADS1014/ADS1015 I2C ADC
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2016, 2018 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2020 Lucio Tarantino <https://github.com/dianlight>
 *
 * BSD Licensed as described in the file LICENSE
 */

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ads111x.h"

/*
 * The datasheet guarantees 400 kHz in the fast mode and 3.4 MHz only in the
 * high-speed mode, which the master enters by sending the high-speed master
 * code 00001xxx before the address; the I2C specification forbids
 * acknowledging that code, and the I2C driver has no API to send such a
 * header, so the high-speed mode is out of reach. The chip runs at 1 MHz in practice, which is what this driver uses;
 * lower it here or through dev->cfg.master.clk_speed on a slow bus.
 */
#ifndef ADS111X_I2C_FREQ_HZ
#define ADS111X_I2C_FREQ_HZ 1000000
#endif

#define REG_CONVERSION 0
#define REG_CONFIG     1
#define REG_THRESH_L   2
#define REG_THRESH_H   3

#define COMP_QUE_OFFSET  0
#define COMP_QUE_MASK    0x03
#define COMP_LAT_OFFSET  2
#define COMP_LAT_MASK    0x01
#define COMP_POL_OFFSET  3
#define COMP_POL_MASK    0x01
#define COMP_MODE_OFFSET 4
#define COMP_MODE_MASK   0x01
#define DR_OFFSET        5
#define DR_MASK          0x07
#define MODE_OFFSET      8
#define MODE_MASK        0x01
#define PGA_OFFSET       9
#define PGA_MASK         0x07
#define MUX_OFFSET       12
#define MUX_MASK         0x07
#define OS_OFFSET        15
#define OS_MASK          0x01

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static const char *TAG = "ads111x";

const float ads111x_gain_values[] =
{
    [ADS111X_GAIN_6V144]   = 6.144,
    [ADS111X_GAIN_4V096]   = 4.096,
    [ADS111X_GAIN_2V048]   = 2.048,
    [ADS111X_GAIN_1V024]   = 1.024,
    [ADS111X_GAIN_0V512]   = 0.512,
    [ADS111X_GAIN_0V256]   = 0.256,
    [ADS111X_GAIN_0V256_2] = 0.256,
    [ADS111X_GAIN_0V256_3] = 0.256
};

static esp_err_t read_reg(i2c_dev_t *dev, uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    esp_err_t res;
    if ((res = i2c_dev_read_reg(dev, reg, buf, 2)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read from register 0x%02x", reg);
        return res;
    }
    *val = (buf[0] << 8) | buf[1];

    return ESP_OK;
}

static esp_err_t write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { val >> 8, val };
    esp_err_t res;
    if ((res = i2c_dev_write_reg(dev, reg, buf, 2)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not write 0x%04x to register 0x%02x", val, reg);
        return res;
    }

    return ESP_OK;
}

static esp_err_t read_conf_bits(i2c_dev_t *dev, uint8_t offs, uint16_t mask,
                                uint16_t *bits)
{
    CHECK_ARG(dev);

    uint16_t val;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_CONFIG, &val));
    I2C_DEV_GIVE_MUTEX(dev);

    ESP_LOGD(TAG, "Got config value: 0x%04x", val);

    *bits = (val >> offs) & mask;

    return ESP_OK;
}

static esp_err_t write_conf_bits(i2c_dev_t *dev, uint16_t val, uint8_t offs,
                                 uint16_t mask)
{
    CHECK_ARG(dev);

    uint16_t old;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_CONFIG, &old));
    // Issue #593
    // Writing a configuration field must not start a conversion as a side
    // effect: the OS bit reads 1 while the chip is idle, so a plain
    // read-modify-write would put it back and begin a conversion.
    if (offs != OS_OFFSET || mask != OS_MASK)
        old &= ~(OS_MASK << OS_OFFSET);
    I2C_DEV_CHECK(dev, write_reg(dev, REG_CONFIG, (old & ~(mask << offs)) | (val << offs)));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

#define READ_CONFIG(OFFS, MASK, VAR) do { \
        CHECK_ARG(VAR); \
        uint16_t bits; \
        CHECK(read_conf_bits(dev, OFFS, MASK, &bits)); \
        *VAR = bits; \
        return ESP_OK; \
    } while(0)


///////////////////////////////////////////////////////////////////////////////

esp_err_t ads111x_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port,
                            gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);
    CHECK_ARG(port >= 0 && port < I2C_NUM_MAX);
    CHECK_ARG(GPIO_IS_VALID_OUTPUT_GPIO(sda_gpio));
    CHECK_ARG(GPIO_IS_VALID_OUTPUT_GPIO(scl_gpio));
    CHECK_ARG(sda_gpio != scl_gpio);

    if (addr != ADS111X_ADDR_GND && addr != ADS111X_ADDR_VCC
            && addr != ADS111X_ADDR_SDA && addr != ADS111X_ADDR_SCL)
    {
        ESP_LOGE(TAG, "Invalid I2C address");
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->addr = addr;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
    // The pull-up flags are left as the caller has set them: the chip needs
    // external resistors anyway, and the internal ones are only a fallback.
    if (!dev->cfg.master.clk_speed)
        dev->cfg.master.clk_speed = ADS111X_I2C_FREQ_HZ;
    return i2c_dev_create_mutex(dev);
}

esp_err_t ads111x_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);

    esp_err_t res = i2c_dev_delete_mutex(dev);
    if (res == ESP_OK)
    {
        dev->mutex = NULL;
        dev->dev_handle = NULL;
    }

    return res;
}

esp_err_t ads111x_is_busy(i2c_dev_t *dev, bool *busy)
{
    CHECK_ARG(dev && busy);

    uint16_t r;
    CHECK(read_conf_bits(dev, OS_OFFSET, OS_MASK, &r));
    *busy = !r;

    return ESP_OK;
}

esp_err_t ads111x_wait_conversion(i2c_dev_t *dev, uint32_t timeout_ms)
{
    CHECK_ARG(dev);
    CHECK_ARG(timeout_ms > 0);

    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    if (!timeout)
        timeout = 1;

    TickType_t start = xTaskGetTickCount();
    bool busy;

    for (;;)
    {
        CHECK(ads111x_is_busy(dev, &busy));
        if (!busy)
            return ESP_OK;

        if (xTaskGetTickCount() - start >= timeout)
        {
            ESP_LOGE(TAG, "The conversion did not finish in %u ms", (unsigned)timeout_ms);
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(1);
    }
}

esp_err_t ads111x_start_conversion(i2c_dev_t *dev)
{
    return write_conf_bits(dev, 1, OS_OFFSET, OS_MASK);
}

esp_err_t ads111x_get_value(i2c_dev_t *dev, int16_t *value)
{
    CHECK_ARG(dev && value);

    uint16_t raw;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_CONVERSION, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *value = (int16_t)raw;

    return ESP_OK;
}

esp_err_t ads101x_get_value(i2c_dev_t *dev, int16_t *value)
{
    CHECK_ARG(dev && value);

    uint16_t raw;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_CONVERSION, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    // the result is left-justified in the 16-bit register: the upper 12 bits
    // hold a signed value, so convert it to a signed 16-bit value by hand
    int32_t v = raw >> 4;
    if (v & 0x0800)
        v -= 0x1000;
    *value = (int16_t)v;

    return ESP_OK;
}

esp_err_t ads111x_get_gain(i2c_dev_t *dev, ads111x_gain_t *gain)
{
    READ_CONFIG(PGA_OFFSET, PGA_MASK, gain);
}

esp_err_t ads111x_set_gain(i2c_dev_t *dev, ads111x_gain_t gain)
{
    CHECK_ARG(dev && gain >= ADS111X_GAIN_6V144 && gain <= ADS111X_GAIN_0V256_3);

    return write_conf_bits(dev, gain, PGA_OFFSET, PGA_MASK);
}

esp_err_t ads111x_get_input_mux(i2c_dev_t *dev, ads111x_mux_t *mux)
{
    READ_CONFIG(MUX_OFFSET, MUX_MASK, mux);
}

esp_err_t ads111x_set_input_mux(i2c_dev_t *dev, ads111x_mux_t mux)
{
    CHECK_ARG(dev && mux >= ADS111X_MUX_0_1 && mux <= ADS111X_MUX_3_GND);

    return write_conf_bits(dev, mux, MUX_OFFSET, MUX_MASK);
}

esp_err_t ads111x_get_mode(i2c_dev_t *dev, ads111x_mode_t *mode)
{
    READ_CONFIG(MODE_OFFSET, MODE_MASK, mode);
}

esp_err_t ads111x_set_mode(i2c_dev_t *dev, ads111x_mode_t mode)
{
    CHECK_ARG(dev && mode >= ADS111X_MODE_CONTINUOUS && mode <= ADS111X_MODE_SINGLE_SHOT);

    return write_conf_bits(dev, mode, MODE_OFFSET, MODE_MASK);
}

esp_err_t ads111x_get_data_rate(i2c_dev_t *dev, ads111x_data_rate_t *rate)
{
    READ_CONFIG(DR_OFFSET, DR_MASK, rate);
}

esp_err_t ads111x_set_data_rate(i2c_dev_t *dev, ads111x_data_rate_t rate)
{
    CHECK_ARG(dev && rate >= ADS111X_DATA_RATE_8 && rate <= ADS111X_DATA_RATE_860);

    return write_conf_bits(dev, rate, DR_OFFSET, DR_MASK);
}

esp_err_t ads111x_get_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t *mode)
{
    READ_CONFIG(COMP_MODE_OFFSET, COMP_MODE_MASK, mode);
}

esp_err_t ads111x_set_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t mode)
{
    CHECK_ARG(dev && mode >= ADS111X_COMP_MODE_NORMAL && mode <= ADS111X_COMP_MODE_WINDOW);

    return write_conf_bits(dev, mode, COMP_MODE_OFFSET, COMP_MODE_MASK);
}

esp_err_t ads111x_get_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t *polarity)
{
    READ_CONFIG(COMP_POL_OFFSET, COMP_POL_MASK, polarity);
}

esp_err_t ads111x_set_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t polarity)
{
    CHECK_ARG(dev && polarity >= ADS111X_COMP_POLARITY_LOW && polarity <= ADS111X_COMP_POLARITY_HIGH);

    return write_conf_bits(dev, polarity, COMP_POL_OFFSET, COMP_POL_MASK);
}

esp_err_t ads111x_get_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t *latch)
{
    READ_CONFIG(COMP_LAT_OFFSET, COMP_LAT_MASK, latch);
}

esp_err_t ads111x_set_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t latch)
{
    CHECK_ARG(dev && latch >= ADS111X_COMP_LATCH_DISABLED && latch <= ADS111X_COMP_LATCH_ENABLED);

    return write_conf_bits(dev, latch, COMP_LAT_OFFSET, COMP_LAT_MASK);
}

esp_err_t ads111x_get_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t *queue)
{
    READ_CONFIG(COMP_QUE_OFFSET, COMP_QUE_MASK, queue);
}

esp_err_t ads111x_set_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t queue)
{
    CHECK_ARG(dev && queue >= ADS111X_COMP_QUEUE_1 && queue <= ADS111X_COMP_QUEUE_DISABLED);

    return write_conf_bits(dev, queue, COMP_QUE_OFFSET, COMP_QUE_MASK);
}

esp_err_t ads111x_get_comp_low_thresh(i2c_dev_t *dev, int16_t *th)
{
    CHECK_ARG(dev && th);

    uint16_t raw;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_THRESH_L, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *th = (int16_t)raw;

    return ESP_OK;
}

esp_err_t ads111x_set_comp_low_thresh(i2c_dev_t *dev, int16_t th)
{
    CHECK_ARG(dev);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, write_reg(dev, REG_THRESH_L, (uint16_t)th));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

esp_err_t ads111x_get_comp_high_thresh(i2c_dev_t *dev, int16_t *th)
{
    CHECK_ARG(dev && th);

    uint16_t raw;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, REG_THRESH_H, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *th = (int16_t)raw;

    return ESP_OK;
}

esp_err_t ads111x_set_comp_high_thresh(i2c_dev_t *dev, int16_t th)
{
    CHECK_ARG(dev);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, write_reg(dev, REG_THRESH_H, (uint16_t)th));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}
