# TGRK
(Tiny General Reusable Keywords) An Interpreter for Attiny85

This runs on the [Tiny Bit Machine](https://hackaday.io/project/197267-tiny-bit-machine).

Setup: 
* Install Tinycore for attiny85: [https://github.com/SpenceKonde/ATTinyCore](https://github.com/SpenceKonde/ATTinyCore)
* "Burn Bootloader" using ISCP to attiny85:
  * Board: Attiny25/45/85(No Bootloader)
    * BOD: Disabled
    * Chip: Attiny85
    * Clock Source: 1mhz Internal
    * Timer 1 Clock: CPU frequency
* Change ADC values in "buttons()" function according to your button resistor ladder values,
flash over ISCP
* Begin writing binary TGRK code! Use the TGRK_KEYWORDS.pdf cheat sheet.
