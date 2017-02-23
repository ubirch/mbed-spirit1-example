# mbed-os SPIRIT1 example

>  The tool blhost (Kinetis bootloader helper) should be in the PATH.

```
mbed compile
blhost -u -- flash-erase-all
blhost -u -- write-memory 0x0 BUILD/UBIRCH1/GCC_ARM/mbed-os-blinky.bin
blhost -u -- reset
```
