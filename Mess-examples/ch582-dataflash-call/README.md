# CH582: call a function from DataFlash

The CodeFlash application calls a function at `0x00070000`, the start of the
CH582's 32 KB DataFlash region. The function image is built separately from the
application. The function takes three `uint32_t` arguments. Variant `v1` adds
them; variant `v2` doubles the second argument before adding them.

Initially, flash the app and variant `v1` together:

```sh
make flash
```

The app passes `10`, `20`, and `12`, so it prints 42. Then update only
DataFlash with `v2`:

```sh
make DATAFLASH_VARIANT=v2 flash-dataflash
```

Reset the chip. The same CodeFlash app should now print 62. No app rebuild or
CodeFlash write is needed for this replacement. Connect the CH582 through
minichlink when running either flash command.
