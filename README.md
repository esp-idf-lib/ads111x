# ADS111x driver for ESP-IDF

[![Build](https://github.com/esp-idf-lib/ads111x/actions/workflows/build.yml/badge.svg)](https://github.com/esp-idf-lib/ads111x/actions/workflows/build.yml)
[![Documentation](https://github.com/esp-idf-lib/ads111x/actions/workflows/build-docs.yml/badge.svg)](https://github.com/esp-idf-lib/ads111x/actions/workflows/build-docs.yml)
[![Validation](https://github.com/esp-idf-lib/ads111x/actions/workflows/validate-component.yml/badge.svg)](https://github.com/esp-idf-lib/ads111x/actions/workflows/validate-component.yml)
[![Component Registry](https://components.espressif.com/components/esp-idf-lib/ads111x/badge.svg)](https://components.espressif.com/components/esp-idf-lib/ads111x)

Driver for the ADS1113 / ADS1114 / ADS1115 and ADS1013 / ADS1014 / ADS1015
analog-to-digital converters.

* [Documentation](https://esp-idf-lib.github.io/ads111x/)
* [Repository](https://github.com/esp-idf-lib/ads111x)
* [Issues](https://github.com/esp-idf-lib/ads111x/issues)
* [Discussions and questions](https://github.com/esp-idf-lib/core/discussions)
* [Component page at the ESP Component Registry](https://components.espressif.com/components/esp-idf-lib/ads111x)

## Features

* Both families: the 16-bit ADS111x and the 12-bit ADS101x;
* single-shot and continuous conversion modes;
* four single-ended inputs or two differential pairs (ADS1115 and ADS1015);
* gain from ±6.144 V to ±0.256 V, programmable data rate up to 860 samples per
  second;
* the comparator with all its options: mode, polarity, latch, queue and
  thresholds.

## Devices

| Chip      | Bits | Inputs                              | Max data rate |
|-----------|------|-------------------------------------|---------------|
| `ADS1113` | 16   | 1 differential                      | 860 SPS       |
| `ADS1114` | 16   | 1 differential, comparator          | 860 SPS       |
| `ADS1115` | 16   | 4 single-ended / 2 differential     | 860 SPS       |
| `ADS1013` | 12   | 1 differential                      | 3300 SPS      |
| `ADS1014` | 12   | 1 differential, comparator          | 3300 SPS      |
| `ADS1015` | 12   | 4 single-ended / 2 differential     | 3300 SPS      |

## Wiring

The chips speak I²C at the 8-bit address `1001xxx0`, where `xxx` comes from the
`ADDR` pin:

| `ADDR` pin         | 7-bit address | Macro                      |
|--------------------|---------------|----------------------------|
| `GND`              | `0x48`        | `ADS111X_ADDR_GND`         |
| `VDD`              | `0x49`        | `ADS111X_ADDR_VCC`         |
| `SDA`              | `0x4A`        | `ADS111X_ADDR_SDA`         |
| `SCL`              | `0x4B`        | `ADS111X_ADDR_SCL`         |

`VDD` is 2.0–5.5 V, so the chip is happy on the 3.3 V rail. The `ALERT/RDY` pin
is an open-drain output and needs a pull-up if the comparator is used. The
`ADDR` pin must not be left floating.

Connect `SDA` and `SCL` to the ESP32 I²C pins with 4.7 kΩ pull-up resistors to
`VDD` (many breakout boards have them already). The driver leaves
`dev->cfg.sda_pullup_en` and `dev->cfg.scl_pullup_en` as they are, so the
internal pull-ups of the ESP32 can be enabled on a short and slow bus, but they
are not a substitute for the resistors.

## Bus speed

The datasheet describes the fast mode up to 400 kHz and a high-speed mode up to
3.4 MHz. The high-speed mode requires the driver to send the Hs master code
`00001xxx` before every transfer and to switch the bus timing, and the chip does
not acknowledge that code, so it cannot be used with the ESP-IDF I²C master:
the peripheral has no way to send such a header.

The driver talks to the chip at 1 MHz, which is outside the fast mode but works
in practice with short wires and proper pull-ups. Change
`ADS111X_I2C_FREQ_HZ` (or define it in the build) if your wiring is longer or
the chip is shared with slower devices.

## Reading the ADC

The result register holds a 16-bit two's complement value, from `-32768` to
`32767`: the converter measures from the negative full scale to the positive
one, and the positive end stops one LSB short of it. So the full scale is
`32768` codes and the top code is `32767` (`0x7FFF`). On the ADS101x the 12-bit
result is left-justified in the same register, from `-2048` to `2047`, and the
low four bits always read zero.

In the single-shot mode a conversion starts when the OS bit is written to 1 and
the chip powers down when it is done. The driver starts the conversion with
`ads111x_start_conversion()`, which returns at once; the result is ready when
the OS bit reads 1, which is exactly what `ads111x_is_busy()` reports. Waiting
by polling needs a timeout, otherwise a missing or stuck chip hangs the caller,
so use `ads111x_wait_conversion()`:

```c
i2c_dev_t dev = { 0 };

ESP_ERROR_CHECK(ads111x_init_desc(&dev, ADS111X_ADDR_GND, 0,
        CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
ESP_ERROR_CHECK(ads111x_set_mode(&dev, ADS111X_MODE_SINGLE_SHOT));
ESP_ERROR_CHECK(ads111x_set_data_rate(&dev, ADS111X_DATA_RATE_128));
ESP_ERROR_CHECK(ads111x_set_input_mux(&dev, ADS111X_MUX_0_GND));
ESP_ERROR_CHECK(ads111x_set_gain(&dev, ADS111X_GAIN_4V096));

int16_t raw;
ESP_ERROR_CHECK(ads111x_start_conversion(&dev));
ESP_ERROR_CHECK(ads111x_wait_conversion(&dev, 100));
ESP_ERROR_CHECK(ads111x_get_value(&dev, &raw));
```

The conversion takes 1 / data rate seconds: 7.8 ms at 128 SPS, 125 ms at 8 SPS.
`ads111x_wait_conversion()` polls the OS bit with one tick steps, so the
timeout should be a few milliseconds longer than the conversion itself.

In the continuous mode the chip converts all the time and the OS bit is always
0, so `ads111x_is_busy()` reports `true` and `ads111x_start_conversion()` has
nothing to start: read the value whenever you like, the register always holds
the last result.

## Gain and voltage

`ads111x_get_value()` returns the raw code; the voltage is
`gain / 32768 * raw`, where the gain is one of the values of
`ads111x_gain_values[]`, taken for the selected `ads111x_gain_t`:

| Gain              | Full scale | LSB        |
|-------------------|------------|------------|
| `ADS111X_GAIN_6V144` | ±6.144 V | 187.5 µV   |
| `ADS111X_GAIN_4V096` | ±4.096 V | 125 µV     |
| `ADS111X_GAIN_2V048` | ±2.048 V | 62.5 µV    |
| `ADS111X_GAIN_1V024` | ±1.024 V | 31.25 µV   |
| `ADS111X_GAIN_0V512` | ±0.512 V | 15.625 µV  |
| `ADS111X_GAIN_0V256` | ±0.256 V | 7.8125 µV  |

The chip cannot measure above its supply voltage: with `VDD` at 3.3 V a signal
of 4 V cannot be read, and the input must stay within `GND - 0.3 V` …
`VDD + 0.3 V`.

## Data rate

`ADS111X_DATA_RATE_8` through `ADS111X_DATA_RATE_860` select 8, 16, 32, 64, 128,
250, 475 or 860 samples per second for the ADS111x, and 128 … 3300 for the
ADS101x. The higher the rate, the larger the noise: the datasheet shows the
effective resolution dropping as the rate grows.

## Comparator

The comparator watches the conversion result against
`ads111x_set_comp_low_thresh()` and `ads111x_set_comp_high_thresh()` and drives
the `ALERT/RDY` pin according to `ads111x_set_comp_mode()`,
`..._comp_polarity()`, `..._comp_latch()` and `..._comp_queue()`.

In the latching mode the pin stays asserted until the conversion register is
read, so the pulse cannot be missed; the non-latching mode follows the input and
drops back as soon as a conversion falls inside the thresholds.

The thresholds are 16-bit codes in the same left-justified format as the result,
not volts: on the ADS101x the chip compares them as 12-bit values, so the low
four bits are ignored. In the usual window mode the high threshold must be
greater than the low one, and the window is entered with the high threshold
having the MSB set and the low threshold cleared, so the driver writes the
values as they are and expects the caller to keep them sensible:

```c
ESP_ERROR_CHECK(ads111x_set_comp_high_thresh(&dev, 1000));
ESP_ERROR_CHECK(ads111x_set_comp_low_thresh(&dev, -1000));
```

The default thresholds are `0x8000` (low) and `0x7FFF` (high) — the whole
range, so the comparator never fires until they are changed.

## Thread safety

The driver takes a per-device mutex around every register access, so the same
descriptor may be shared between tasks. `ads111x_free_desc()` does not take the
mutex: do not delete a descriptor while another task is using it.

## API

| Function | Description |
|----------|-------------|
| `ads111x_init_desc()` | Fill a descriptor and create its mutex |
| `ads111x_free_desc()` | Delete the mutex and clear the descriptor |
| `ads111x_is_busy()` | Read the OS bit: true while a conversion runs |
| `ads111x_wait_conversion()` | Poll the OS bit until the conversion ends, with a timeout |
| `ads111x_start_conversion()` | Set the OS bit, starting a conversion in the single-shot mode |
| `ads111x_get_value()` | Read the 16-bit result of the ADS111x |
| `ads101x_get_value()` | Read the 12-bit result of the ADS101x |
| `ads111x_get_gain()`, `ads111x_set_gain()` | Programmable gain amplifier |
| `ads111x_get_input_mux()`, `ads111x_set_input_mux()` | Input selection of the ADS1115 |
| `ads111x_get_mode()`, `ads111x_set_mode()` | Continuous or single-shot conversion |
| `ads111x_get_data_rate()`, `ads111x_set_data_rate()` | Samples per second |
| `ads111x_get_comp_mode()`, `ads111x_set_comp_mode()` | Traditional or window comparator |
| `ads111x_get_comp_polarity()`, `ads111x_set_comp_polarity()` | Active low or active high `ALERT/RDY` |
| `ads111x_get_comp_latch()`, `ads111x_set_comp_latch()` | Latching or transparent output |
| `ads111x_get_comp_queue()`, `ads111x_set_comp_queue()` | How many conversions to wait for |
| `ads111x_get_comp_low_thresh()`, `ads111x_set_comp_low_thresh()` | Low threshold |
| `ads111x_get_comp_high_thresh()`, `ads111x_set_comp_high_thresh()` | High threshold |

## Example

See [examples/default](examples/default). It reads two devices on one bus,
one at the address 0x48 and one at 0x49, and in a loop walks over the four
single-ended inputs of each device: it switches the multiplexer, starts a
conversion, waits for it with a timeout and prints the raw values and the
voltages.

## Installation

1. Install the component from the registry:

```sh
idf.py add-dependency "esp-idf-lib/ads111x^1.2.0"
```

2. Or clone the repository into the `components` directory of your project:

```sh
git clone https://github.com/esp-idf-lib/ads111x.git components/ads111x
```

## Support

* [Issues](https://github.com/esp-idf-lib/ads111x/issues)
* [Discussions and questions](https://github.com/esp-idf-lib/core/discussions) at
  [esp-idf-lib/core](https://github.com/esp-idf-lib/core).

## Contributing

Please read [CONTRIBUTING.md](https://github.com/esp-idf-lib/core/blob/main/CONTRIBUTING.md)
at [esp-idf-lib/core](https://github.com/esp-idf-lib/core).
