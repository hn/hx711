# HX711

Raspberry Pi HX711 weight scale interface

This thing interfaces with the HX711 24-Bit Analog-to-Digital Converter (ADC) for Weigh Scales (http://www.aviaic.com/uploadfile/hx711_brief_en.pdf). You can buy cheap breakout boards at Amazon ("_5PCS New Weighing Sensor AD Module Dual-channel 24-bit A/D Conversion HX711_" = $9).

## Installation

### GPIO pins

At the top of HX711.c there two defines, which you need to adjust as the wires are connected to your Pi:

```
#define CLOCK_PIN       24
#define DATA_PIN        23
```

`CLOCK` pin goes to `PD_CLK` (pin 11 on HX711, and SCK pin on the breakout board).
`DATA`  pin goes to `DOUT` (pin 12 on HX711, and DT pin on the breakout board).

### Raspberry Pi 2

This software uses the [Gertboard](https://www.raspberrypi.org/blog/gertboard-is-here/) code to access the GPIOs of the Raspberry. Gertboard uses hard-coded peripheral addresses, however the address has changed between Pi and Pi2. So to get the code to run on a Raspberry Pi 2, the `BCM2708_PERI_BASE` has to be changed (in `gb_common.c`):

```
// Raspberry Pi
#define BCM2708_PERI_BASE        0x20000000
// Raspberry Pi 2
#define BCM2708_PERI_BASE        0x3F000000
```

### Compilation

Compile by running make, which effectively does: 
```
gcc -o HX711 HX711.c gb_common.c
```

## Usage

### First run (zero/tare)

Start the first run without a weight on the scale. If there's nothing on the scale, this is the zero, tare, whatever. The number itself is a 24bit representation that comes off the converter:

```
root@raspberrypi:~/hx711# ./hx711 -vv
raw reading: 11111111 111111100110010101011000 =    -105128
raw reading: 11111111 111111100110010100011110 =    -105186
raw reading: 11111111 111111100110010100000100 =    -105212
[...]
raw reading: 11111111 111111100110010100100110 =    -105178
raw reading: 11111111 111111100110010100011010 =    -105190
raw reading: 11111111 111111100110010100110010 =    -105166
raw average: -105186
filter average within 5.00 percent range (-110445 ... -99927): -105186 from 64 samples
calibration offset: 0, calibration weight: 0, calibration value: 0.
-105186
```

It's perfectly fine to have a negative numbers here (`-105186` in this case).

### Second run (calibration)

To actually be able to convert to any usable reading, you would have to calibrate the scale using a known weight. So, for example, you put a `1000g` weight on the scale and start a reading with the offset measured during the first run:

```
root@raspberrypi:~/hx711# ./hx711 -vv -o-105186
raw reading: 00000000 000000011011110000100000 =     113696
raw reading: 00000000 000000011011101111111010 =     113658
raw reading: 00000000 000000011011101111111010 =     113658
[...]
raw reading: 00000000 000000011011101111011100 =     113628
raw reading: 00000000 000000011011110000001110 =     113678
raw reading: 00000000 000000011011101111101010 =     113642
raw average: 113632
filter average within 5.00 percent range (107951 ... 119313): 113632 from 64 samples
calibration offset: -105186, calibration weight: 0, calibration value: 0.
218818
```

### Subsequent runs (measurements)

Now that you know that your zero offset is `-105186` and your calibration reading for a `1000g` weight is `218818`, you can start your first real measurement with an unknown weight:

```
root@raspberrypi:~/hx711# ./hx711 -vv -o-105186 -w1000 -c218818
raw reading: 00000000 000000001010011010010110 =      42646
raw reading: 00000000 000000001010011000101000 =      42536
raw reading: 00000000 000000001010011000010110 =      42518
[...]
raw reading: 00000000 000000001010011001010110 =      42582
raw reading: 00000000 000000001010011001000100 =      42564
raw reading: 00000000 000000001010011001010100 =      42580
raw average: 42588
filter average within 5.00 percent range (40459 ... 44717): 42588 from 64 samples
calibration offset: -105186, calibration weight: 1000, calibration value: 218818.
675.33
```

Et voila ... the unknown weight is `675g`.

## Credits

All the initial hard work has been done by https://github.com/ggurov.

Code indentation, getopt, calibration maths has been added by https://github.com/hn.

Some code has been borrowed from the Gertboard distribution to define the memory locations for GPIO_SET, CLR, and IN0, gpio setup, etc.

## Misc

The load cell will drift depending on temperature and humidity.

One note to keep in mind, this code is designed to run at 80 samples per second, the breakout board comes defaulted to 10 samples per second, This will manifest itself as the whole process taking a fairly long time to complete. To switch to 80 samples per second, one has to desolder the RATE pin from the pad it's soldered to, lift it and solder it to +VCC on HX711 breakout board (pin 15). By default, this pin is grounded, which sets HX711 to run at 10 samples per second.

Inherent problem with Raspberry Pi's GPIO and talking to something that requires strict timing like the HX711 chip, is that linux running on the Raspberry Pi is not a realtime operating system. This can cause errors here and there, so there's a few safeguards in the code to reset the chip, etc etc.
