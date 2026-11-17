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
 * @file ads111x.h
 * @defgroup ads111x ads111x
 * @{
 *
 * ESP-IDF driver for ADS1113/ADS1114/ADS1115, ADS1013/ADS1014/ADS1015 I2C ADC
 *
 * Version: 1.2.0
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2016, 2018 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2020 Lucio Tarantino <https://github.com/dianlight>
 *
 * BSD Licensed as described in the file LICENSE
 */
#ifndef __ADS111X_H__
#define __ADS111X_H__

#include <stdbool.h>
#include <esp_err.h>
#include <i2cdev.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS111X_ADDR_GND 0x48 //!< I2C device address with ADDR pin connected to ground
#define ADS111X_ADDR_VCC 0x49 //!< I2C device address with ADDR pin connected to VCC
#define ADS111X_ADDR_SDA 0x4a //!< I2C device address with ADDR pin connected to SDA
#define ADS111X_ADDR_SCL 0x4b //!< I2C device address with ADDR pin connected to SCL

#define ADS111X_MAX_VALUE 0x7fff //!< Maximum ADC value
#define ADS101X_MAX_VALUE 0x7ff

// ADS101X overrides
#define ADS101X_DATA_RATE_128   ADS111X_DATA_RATE_8
#define ADS101X_DATA_RATE_250   ADS111X_DATA_RATE_16
#define ADS101X_DATA_RATE_490   ADS111X_DATA_RATE_32
#define ADS101X_DATA_RATE_920   ADS111X_DATA_RATE_64
#define ADS101X_DATA_RATE_1600  ADS111X_DATA_RATE_128
#define ADS101X_DATA_RATE_2400  ADS111X_DATA_RATE_250
#define ADS101X_DATA_RATE_3300  ADS111X_DATA_RATE_475

/**
 * Gain amplifier
 */
typedef enum
{
    ADS111X_GAIN_6V144 = 0, //!< +-6.144V
    ADS111X_GAIN_4V096,     //!< +-4.096V
    ADS111X_GAIN_2V048,     //!< +-2.048V (default)
    ADS111X_GAIN_1V024,     //!< +-1.024V
    ADS111X_GAIN_0V512,     //!< +-0.512V
    ADS111X_GAIN_0V256,     //!< +-0.256V
    ADS111X_GAIN_0V256_2,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
    ADS111X_GAIN_0V256_3,   //!< +-0.256V (same as ADS111X_GAIN_0V256)
} ads111x_gain_t;

/**
 * Gain amplifier values
 */
extern const float ads111x_gain_values[];

/**
 * Input multiplexer configuration (ADS1115 only)
 */
typedef enum
{
    ADS111X_MUX_0_1 = 0, //!< positive = AIN0, negative = AIN1 (default)
    ADS111X_MUX_0_3,     //!< positive = AIN0, negative = AIN3
    ADS111X_MUX_1_3,     //!< positive = AIN1, negative = AIN3
    ADS111X_MUX_2_3,     //!< positive = AIN2, negative = AIN3
    ADS111X_MUX_0_GND,   //!< positive = AIN0, negative = GND
    ADS111X_MUX_1_GND,   //!< positive = AIN1, negative = GND
    ADS111X_MUX_2_GND,   //!< positive = AIN2, negative = GND
    ADS111X_MUX_3_GND,   //!< positive = AIN3, negative = GND
} ads111x_mux_t;

/**
 * Data rate
 */
typedef enum
{
    ADS111X_DATA_RATE_8 = 0, //!< 8 samples per second
    ADS111X_DATA_RATE_16,    //!< 16 samples per second
    ADS111X_DATA_RATE_32,    //!< 32 samples per second
    ADS111X_DATA_RATE_64,    //!< 64 samples per second
    ADS111X_DATA_RATE_128,   //!< 128 samples per second (default)
    ADS111X_DATA_RATE_250,   //!< 250 samples per second
    ADS111X_DATA_RATE_475,   //!< 475 samples per second
    ADS111X_DATA_RATE_860    //!< 860 samples per second
} ads111x_data_rate_t;

/**
 * Operational mode
 */
typedef enum
{
    ADS111X_MODE_CONTINUOUS = 0, //!< Continuous conversion mode
    ADS111X_MODE_SINGLE_SHOT    //!< Power-down single-shot mode (default)
} ads111x_mode_t;

/**
 * Comparator mode (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_MODE_NORMAL = 0, //!< Traditional comparator with hysteresis (default)
    ADS111X_COMP_MODE_WINDOW      //!< Window comparator
} ads111x_comp_mode_t;

/**
 * Comparator polarity (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_POLARITY_LOW = 0, //!< Active low (default)
    ADS111X_COMP_POLARITY_HIGH     //!< Active high
} ads111x_comp_polarity_t;

/**
 * Comparator latch (ADS1114 and ADS1115 only)
 */
typedef enum
{
    ADS111X_COMP_LATCH_DISABLED = 0, //!< Non-latching comparator (default)
    ADS111X_COMP_LATCH_ENABLED       //!< Latching comparator
} ads111x_comp_latch_t;

/**
 * Comparator queue
 */
typedef enum
{
    ADS111X_COMP_QUEUE_1 = 0,   //!< Assert ALERT/RDY pin after one conversion
    ADS111X_COMP_QUEUE_2,       //!< Assert ALERT/RDY pin after two conversions
    ADS111X_COMP_QUEUE_4,       //!< Assert ALERT/RDY pin after four conversions
    ADS111X_COMP_QUEUE_DISABLED //!< Disable comparator (default)
} ads111x_comp_queue_t;

/**
 * @brief Initialize device descriptor
 *
 * @param dev Device descriptor
 * @param addr Device address
 * @param port I2C port number
 * @param sda_gpio GPIO pin for SDA
 * @param scl_gpio GPIO pin for SCL
 *
 * Sets the bus speed to ::ADS111X_I2C_FREQ_HZ unless `dev->cfg.master.clk_speed`
 * already holds a value, and does not touch the pull-up flags: the bus needs
 * external resistors, and the internal ones are only a fallback for a short
 * and slow bus. Both can be set in `dev->cfg` before this call or changed after
 * it and before the first operation.
 *
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the port is out of range, a pin is not an
 *        output-capable GPIO or the pins are equal, or the address is not one
 *        of ::ADS111X_ADDR_GND,
 *        ::ADS111X_ADDR_VCC, ::ADS111X_ADDR_SDA, ::ADS111X_ADDR_SCL
 */
esp_err_t ads111x_init_desc(i2c_dev_t *dev, uint8_t addr, i2c_port_t port,
                            gpio_num_t sda_gpio, gpio_num_t scl_gpio);

/**
 * @brief Free device descriptor
 *
 * Releases the device handle and the mutex. The descriptor is not reusable
 * after this call: initialize it again with ::ads111x_init_desc().
 *
 * @param dev Device descriptor
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL`
 */
esp_err_t ads111x_free_desc(i2c_dev_t *dev);

/**
 * @brief Get device operational status
 *
 * Reads the OS bit of the configuration register: on read it is 0 while the
 * device is performing a conversion and 1 when the conversion is complete.
 *
 * In the continuous mode the device converts back to back, so the bit always
 * reads 0 and this function cannot be used to tell whether a fresh sample is
 * available; read the conversion register at the pace of the data rate
 * instead.
 *
 * @param dev Device descriptor
 * @param[out] busy True while the device is performing a conversion
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if an argument is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_is_busy(i2c_dev_t *dev, bool *busy);

/**
 * @brief Wait for the current conversion to finish
 *
 * Polls ::ads111x_is_busy() once per tick until the OS bit reports that the
 * conversion is complete. Useful in the single-shot mode, where the result
 * must not be read before the conversion ends. In the continuous mode the bit
 * always reads 0, so the function returns at once and tells nothing about the
 * age of the sample.
 *
 * The timeout should exceed the conversion time of the configured data rate:
 * the rate of 8 samples per second takes 125 ms, 860 samples per second takes
 * 1.2 ms.
 *
 * @param dev Device descriptor
 * @param timeout_ms Timeout in milliseconds, must be greater than zero
 * @return
 *      - `ESP_OK` when the conversion is complete
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or the timeout is zero
 *      - `ESP_ERR_TIMEOUT` if the conversion did not finish in time
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_wait_conversion(i2c_dev_t *dev, uint32_t timeout_ms);

/**
 * @brief Begin a single conversion
 *
 * Only in single-shot mode: the OS bit can be written only while the device
 * is powered down, so in the continuous mode the call does nothing and still
 * reports `ESP_OK`. After the call, wait for the conversion with
 * ::ads111x_wait_conversion() before reading the result, otherwise the
 * previous conversion is returned.
 *
 * @param dev Device descriptor
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_start_conversion(i2c_dev_t *dev);

/**
 * @brief Read last conversion result
 *
 * Returns the whole 16-bit conversion register as a signed value. In the
 * single-shot mode the register holds the previous conversion until the
 * current one completes (or zero after power up).
 *
 * @param dev Device descriptor
 * @param[out] value Last conversion result, from -32768 to 32767
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if an argument is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_get_value(i2c_dev_t *dev, int16_t *value);

/**
 * @brief Read last conversion result for ADS101x
 *
 * The 12-bit result is left-justified in the 16-bit register. The register is
 * read as a whole, the lower four bits are dropped and the sign of the
 * remaining value is extended, so the result is a signed 12-bit value.
 *
 * @param dev Device descriptor
 * @param[out] value Last conversion result, from -2048 to 2047
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if an argument is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads101x_get_value(i2c_dev_t *dev, int16_t *value);

/**
 * @brief Read the programmable gain amplifier configuration
 *
 * ADS1114 and ADS1115 only.
 * Use ::ads111x_gain_values[] for real voltage.
 *
 * @param dev Device descriptor
 * @param[out] gain Gain value
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_gain(i2c_dev_t *dev, ads111x_gain_t *gain);

/**
 * @brief Configure the programmable gain amplifier
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param gain Gain value
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_gain(i2c_dev_t *dev, ads111x_gain_t gain);

/**
 * @brief Read the input multiplexer configuration
 *
 * ADS1115 only.
 *
 * @param dev Device descriptor
 * @param[out] mux Input multiplexer configuration
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_input_mux(i2c_dev_t *dev, ads111x_mux_t *mux);

/**
 * @brief Configure the input multiplexer configuration
 *
 * ADS1115 only.
 *
 * @param dev Device descriptor
 * @param mux Input multiplexer configuration
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_input_mux(i2c_dev_t *dev, ads111x_mux_t mux);

/**
 * @brief Read the device operating mode
 *
 * @param dev Device descriptor
 * @param[out] mode Device operating mode
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_mode(i2c_dev_t *dev, ads111x_mode_t *mode);

/**
 * @brief Set the device operating mode
 *
 * @param dev Device descriptor
 * @param mode Device operating mode
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_mode(i2c_dev_t *dev, ads111x_mode_t mode);

/**
 * @brief Read the data rate
 *
 * @param dev Device descriptor
 * @param[out] rate Data rate
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_data_rate(i2c_dev_t *dev, ads111x_data_rate_t *rate);

/**
 * @brief Configure the data rate
 *
 * @param dev Device descriptor
 * @param rate Data rate
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_data_rate(i2c_dev_t *dev, ads111x_data_rate_t rate);

/**
 * @brief Get comparator mode
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param[out] mode Comparator mode
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t *mode);

/**
 * @brief Set comparator mode
 *
 * In the traditional mode the ALERT/RDY pin asserts when the result rises
 * above the upper threshold and deasserts when it falls below the lower one.
 * In the window mode the pin asserts outside the window, that is, when the
 * result goes above the upper threshold or below the lower one.
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param mode Comparator mode
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_comp_mode(i2c_dev_t *dev, ads111x_comp_mode_t mode);

/**
 * @brief Get polarity of the comparator output pin ALERT/RDY
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param[out] polarity Comparator output pin polarity
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t *polarity);

/**
 * @brief Set polarity of the comparator output pin ALERT/RDY
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param polarity Comparator output pin polarity
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_comp_polarity(i2c_dev_t *dev, ads111x_comp_polarity_t polarity);

/**
 * @brief Get comparator output latch mode
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param[out] latch Comparator output latch mode
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t *latch);

/**
 * @brief Set comparator output latch mode
 *
 * ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param latch Comparator output latch mode
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_comp_latch(i2c_dev_t *dev, ads111x_comp_latch_t latch);

/**
 * @brief Get comparator queue size
 *
 * Get number of the comparator conversions before pin ALERT/RDY
 * assertion. ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param[out] queue Number of the comparator conversions
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t *queue);

/**
 * @brief Set comparator queue size
 *
 * Set number of the comparator conversions before pin ALERT/RDY
 * assertion or disable comparator. ADS1114 and ADS1115 only.
 *
 * @param dev Device descriptor
 * @param queue Number of the comparator conversions
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL` or
 *        the value is outside the enumerated range
 */
esp_err_t ads111x_set_comp_queue(i2c_dev_t *dev, ads111x_comp_queue_t queue);

/**
 * @brief Get the lower threshold value used by comparator
 *
 * The threshold is a signed 16-bit value in the ADC codes of the current gain.
 * On the ADS101x the chip compares it as a left-justified 12-bit value, so the
 * meaningful range is the top twelve bits. To use the pin as a conversion-ready
 * output, set the upper threshold MSB to 1 and the lower one to 0.
 *
 * @param dev Device descriptor
 * @param[out] th Lower threshold value
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_low_thresh(i2c_dev_t *dev, int16_t *th);

/**
 * @brief Set the lower threshold value used by comparator
 *
 * See ::ads111x_get_comp_low_thresh() for the format of the value.
 *
 * @param dev Device descriptor
 * @param th Lower threshold value
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_set_comp_low_thresh(i2c_dev_t *dev, int16_t th);

/**
 * @brief Get the upper threshold value used by comparator
 *
 * See ::ads111x_get_comp_low_thresh() for the format of the value.
 *
 * @param dev Device descriptor
 * @param[out] th Upper threshold value
 * @return `ESP_OK` on success
 */
esp_err_t ads111x_get_comp_high_thresh(i2c_dev_t *dev, int16_t *th);

/**
 * @brief Set the upper threshold value used by comparator
 *
 * See ::ads111x_get_comp_low_thresh() for the format of the value.
 *
 * @param dev Device descriptor
 * @param th Upper threshold value
 * @return
 *      - `ESP_OK` on success
 *      - `ESP_ERR_INVALID_ARG` if the descriptor is `NULL`
 *      - an error code from the I2C layer
 */
esp_err_t ads111x_set_comp_high_thresh(i2c_dev_t *dev, int16_t th);

#ifdef __cplusplus
}
#endif

/**@}*/

#endif /* __ADS111X_H__ */
