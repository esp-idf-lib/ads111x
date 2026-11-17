ads111x: Driver for ADS1113/ADS1114/ADS1115 and ADS1013/ADS1014/ADS1015 I2C ADC
===============================================================================

.. contents::
   :local:
   :depth: 2

Features
--------

* Both families: the 16-bit ADS111x and the 12-bit ADS101x;
* single-shot and continuous conversion modes;
* the input multiplexer, the programmable gain amplifier, the data rate and
  the comparator are configured through the library;
* the result is returned in ADC codes and converted to volts with one
  multiplication;
* thread-safe: every operation is serialized by the device mutex of ``i2cdev``.

Devices
-------

.. list-table::
   :header-rows: 1

   * - Device
     - Resolution
     - Channels
     - Comparator
   * - ADS1113
     - 16 bit
     - 1 differential
     - no
   * - ADS1114
     - 16 bit
     - 1 differential
     - yes
   * - ADS1115
     - 16 bit
     - 2 differential, 4 single-ended
     - yes
   * - ADS1013
     - 12 bit
     - 1 differential
     - no
   * - ADS1014
     - 12 bit
     - 1 differential
     - yes
   * - ADS1015
     - 12 bit
     - 2 differential, 4 single-ended
     - yes

The single-ended inputs are measured against GND with ``ADS111X_MUX_0_GND`` …
``ADS111X_MUX_3_GND``, the differential pairs with ``ADS111X_MUX_0_1``,
``ADS111X_MUX_0_3``, ``ADS111X_MUX_1_3`` and ``ADS111X_MUX_2_3``. The
multiplexer belongs to the ADS1115 and the ADS1015, the gain amplifier to the
ADS1114/ADS1115 and the ADS1014/ADS1015.

Wiring
------

.. list-table::
   :header-rows: 1

   * - Signal
     - Pin
   * - Power
     - ``VDD`` (2.0–5.5 V)
   * - Ground
     - ``GND``
   * - Clock
     - ``SCL``
   * - Data
     - ``SDA``
   * - Ready
     - ``ALERT/RDY`` (optional, open-drain)

The bus needs pull-up resistors, usually 4.7 kOhm; the driver does not enable
the internal pull-ups of the ESP32, but it does not disable them either: the
``dev->cfg.sda_pullup_en`` and ``dev->cfg.scl_pullup_en`` flags are left as the
caller has set them, and they are zero in a zeroed descriptor.

The address is set by the ``ADDR`` pin:

.. list-table::
   :header-rows: 1

   * - ``ADDR``
     - Address
     - Macro
   * - ``GND``
     - 0x48
     - ``ADS111X_ADDR_GND``
   * - ``VDD``
     - 0x49
     - ``ADS111X_ADDR_VCC``
   * - ``SDA``
     - 0x4a
     - ``ADS111X_ADDR_SDA``
   * - ``SCL``
     - 0x4b
     - ``ADS111X_ADDR_SCL``

The analog inputs accept a voltage between ``GND`` and ``VDD``, even when the
gain is set to a smaller range: the range only sets the scale, the input must
stay within the supply rails.

Bus speed
---------

``ads111x_init_desc()`` sets the bus to 1 MHz. The datasheet guarantees
400 kHz in the fast mode; 3.4 MHz belongs to the high-speed mode, which the
master enters by sending the high-speed master code ``00001xxx`` before the
address. The chip does not acknowledge that code, and the ESP-IDF driver has
no way to send it, so the high-speed mode is out of reach and 1 MHz is above
the guaranteed limit, although the chip runs at it in practice.

To change the speed, define ``ADS111X_I2C_FREQ_HZ`` for the component, or set
``dev->cfg.master.clk_speed`` after ``ads111x_init_desc()`` and before the
first operation; ``i2cdev`` applies the configuration when the bus is
created.

Reading the ADC
---------------

In the continuous mode the chip converts back to back and the conversion
register always holds a fresh sample, so the value can be read at any time:

.. code-block:: c

    ESP_ERROR_CHECK(ads111x_set_mode(&dev, ADS111X_MODE_CONTINUOUS));
    ESP_ERROR_CHECK(ads111x_set_data_rate(&dev, ADS111X_DATA_RATE_32));

    int16_t raw;
    ESP_ERROR_CHECK(ads111x_get_value(&dev, &raw));

In the single-shot mode the chip converts on request and then powers down.
The OS bit of the configuration register is the status of that conversion: on
read it is 0 while the conversion is running and 1 when it is complete. The
driver exposes it through ``ads111x_is_busy()``, so the sequence is start,
wait, read:

.. code-block:: c

    ESP_ERROR_CHECK(ads111x_set_mode(&dev, ADS111X_MODE_SINGLE_SHOT));
    ESP_ERROR_CHECK(ads111x_start_conversion(&dev));
    ESP_ERROR_CHECK(ads111x_wait_conversion(&dev, 100));

    int16_t raw;
    ESP_ERROR_CHECK(ads111x_get_value(&dev, &raw));

``ads111x_start_conversion()`` writes the OS bit, which can only be written
while the device is powered down: in the continuous mode the call does
nothing and still returns ``ESP_OK``. ``ads111x_is_busy()`` is therefore
useful only in the single-shot mode; in the continuous mode the bit reads 0
almost all the time and tells nothing about the freshness of the sample.

The ADS101x keeps its 12-bit result left-justified in the 16-bit register,
and ``ads101x_get_value()`` shifts it and extends the sign, so both drivers
return a signed value in the units of the chip: −32768…32767 for the ADS111x
and −2048…2047 for the ADS101x.

Gain and voltage
----------------

The gain sets the full-scale range: one LSB is the gain divided by 32768, and
the codes run from -32768 to 32767, so 32767 is the full scale minus one LSB.
With ``ADS111X_GAIN_4V096`` the top code is 4.0959 V.

.. list-table::
   :header-rows: 1

   * - Gain macro
     - Range
     - LSB (ADS111x)
     - LSB (ADS101x)
   * - ``ADS111X_GAIN_6V144``
     - ±6.144 V
     - 187.5 µV
     - 3 mV
   * - ``ADS111X_GAIN_4V096``
     - ±4.096 V
     - 125 µV
     - 2 mV
   * - ``ADS111X_GAIN_2V048``
     - ±2.048 V
     - 62.5 µV
     - 1 mV
   * - ``ADS111X_GAIN_1V024``
     - ±1.024 V
     - 31.25 µV
     - 0.5 mV
   * - ``ADS111X_GAIN_0V512``
     - ±0.512 V
     - 15.625 µV
     - 0.25 mV
   * - ``ADS111X_GAIN_0V256``
     - ±0.256 V
     - 7.8125 µV
     - 0.125 mV

The gain values are kept in ``ads111x_gain_values[]``, so the voltage is:

.. code-block:: c

    float voltage = ads111x_gain_values[gain] / 32768.0f * raw;

The divisor is the full scale in codes, 32768 for the ADS111x and 2048 for the
ADS101x, not the largest code: dividing by ``ADS111X_MAX_VALUE`` (32767) is off
by one LSB. ``ADS101X_MAX_VALUE`` (2047) is the largest code of the 12-bit
chip, not its full scale either.

Data rate
---------

The converter offers eight rates, from 8 to 860 samples per second
(``ADS111X_DATA_RATE_8`` … ``ADS111X_DATA_RATE_860``). The ADS101x has the
same register values, only the numbers differ: ``ADS101X_DATA_RATE_128`` is
``ADS111X_DATA_RATE_8`` and so on, up to ``ADS101X_DATA_RATE_3300``. The rate
sets how often a sample appears in the continuous mode and how long a
single-shot conversion takes.

Comparator
----------

The ADS1114/ADS1115 and ADS1014/ADS1015 have a comparator that drives the
``ALERT/RDY`` pin. The mode, the polarity, the latch and the queue length are
configured with the corresponding functions. In the latching mode the pin stays
asserted until the conversion register is read, so the pulse cannot be missed;
the non-latching mode follows the input and drops back as soon as a conversion
falls inside the thresholds. The thresholds are signed 16-bit values in ADC
codes of the current gain: on the ADS101x the chip compares them
as left-justified 12-bit values, so only the upper twelve bits matter. To use
the pin as a conversion-ready output instead, set the MSB of the upper
threshold to 1 and the MSB of the lower threshold to 0, as the datasheet
describes.

Example
-------

See ``examples/default``. It reads two devices on one bus, one at the address
0x48 and one at 0x49, prints the raw values and the voltages, and releases the
descriptors at the end.

API reference
-------------

 .. doxygenfile:: ads111x.h
