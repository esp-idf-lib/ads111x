#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ads111x.h>

#define I2C_PORT 0

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

#define GAIN ADS111X_GAIN_4V096 // +-4.096V

// I2C addresses
static const uint8_t addr[CONFIG_EXAMPLE_DEV_COUNT] =
{
    ADS111X_ADDR_GND,
#if CONFIG_EXAMPLE_DEV_COUNT > 1
    ADS111X_ADDR_VCC
#endif
};

// Descriptors
static i2c_dev_t devices[CONFIG_EXAMPLE_DEV_COUNT];

// Gain value
static float gain_val;

static const char *const inputs[] = { "AIN0", "AIN1", "AIN2", "AIN3" };

static const ads111x_mux_t muxes[] =
{
    ADS111X_MUX_0_GND, ADS111X_MUX_1_GND, ADS111X_MUX_2_GND, ADS111X_MUX_3_GND
};

static void measure(size_t n, size_t input)
{
    int16_t raw = 0;
    esp_err_t err = ads111x_set_input_mux(&devices[n], muxes[input]);
    if (err == ESP_OK)
        err = ads111x_start_conversion(&devices[n]);
    if (err == ESP_OK)
        err = ads111x_wait_conversion(&devices[n], 100);
    if (err == ESP_OK)
        err = ads111x_get_value(&devices[n], &raw);

    if (err == ESP_OK)
    {
        float voltage = gain_val / 32768.0f * raw;
        printf("[%zu] %s: raw %6d, %7.04f volts\n", n, inputs[input], raw, voltage);
    }
    else
        printf("[%zu] %s: %s\n", n, inputs[input], esp_err_to_name(err));
}

// Main task
void ads111x_test(void *pvParameters)
{
    gain_val = ads111x_gain_values[GAIN];

    // Setup ICs
    for (size_t i = 0; i < CONFIG_EXAMPLE_DEV_COUNT; i++)
    {
        ESP_ERROR_CHECK(ads111x_init_desc(&devices[i], addr[i], I2C_PORT,
                                          CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));

        ESP_ERROR_CHECK(ads111x_set_mode(&devices[i], ADS111X_MODE_SINGLE_SHOT)); // Single-shot conversion mode
        ESP_ERROR_CHECK(ads111x_set_data_rate(&devices[i], ADS111X_DATA_RATE_32)); // 32 samples per second
        ESP_ERROR_CHECK(ads111x_set_gain(&devices[i], GAIN));
    }

    // In the single-shot mode the chip converts on request and powers down, so
    // each reading is a start, a wait and a read.
    while (true)
    {
        for (size_t input = 0; input < sizeof(inputs) / sizeof(inputs[0]); input++)
            for (size_t i = 0; i < CONFIG_EXAMPLE_DEV_COUNT; i++)
                measure(i, input);

        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main()
{
    // Init library
    ESP_ERROR_CHECK(i2cdev_init());

    // Clear device descriptors
    memset(devices, 0, sizeof(devices));

    // Start task
    xTaskCreatePinnedToCore(ads111x_test, "ads111x_test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);
}
