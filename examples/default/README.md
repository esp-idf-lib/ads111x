# Example for `ADS1115` driver

The datasheet can be found [here](https://www.ti.com/product/ADS1115).

## What it does

The example configures two `ADS1115` devices on an `I2C` bus. One has pin
`ADDR` connected to `GND`, and another has pin `ADDR` connected to `VCC`. Both
devices are switched to the single-shot mode at 32 samples per second and the
gain is ±4.096 V. In a loop, every 500 ms, the example walks over the four
single-ended inputs `AIN0`…`AIN3` against `GND` for every device: it switches
the multiplexer, starts a conversion, waits for the result with a timeout and
prints the raw ADC value together with the voltage. Inputs that are left
floating show noise, which is expected.

## Wiring

Make the connections as below:

| S. No. | `ADS1115` | `ESP32`             |
|--------|-----------|---------------------|
| 1.     | `V_DD`    | `V_in` or 5V source |
| 2.     | `GND`     | `GND`               |
| 3.     | `SCL`     | See below           |
| 4.     | `SDA`     | See below           |
| 5.     | `A0`-`A3` | analog inputs       |

Connect `SCL` and `SDA` pins to the following pins with appropriate pull-up
resistors.

| Name | Description | Defaults |
|------|-------------|----------|
| `CONFIG_EXAMPLE_I2C_MASTER_SCL` | GPIO number for `SCL` | "6" for `esp32c3`, "19" for `esp32`, `esp32s2`, and `esp32s3` |
| `CONFIG_EXAMPLE_I2C_MASTER_SDA` | GPIO number for `SDA` | "5" for `esp32c3`, "18" for `esp32`, `esp32s2`, and `esp32s3` |

This example specifically demonstrates the simultaneous use of multiple
devices, which is why the default `CONFIG_EXAMPLE_DEV_COUNT` is 2. If you are
using only one IC then please change the value of `CONFIG_EXAMPLE_DEV_COUNT`
to 1 under `Example configuration` in `menuconfig`.
