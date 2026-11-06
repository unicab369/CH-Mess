# CH582: call a function from DataFlash

The CodeFlash application calls three functions from separate locations in
the CH582's 32 KB DataFlash region:

- `0x00070400`: adds three scalar arguments (`42` for the sample inputs).
- `0x00070500`: adds the arguments, counting the second one twice (`62`).
- `0x00070600`: takes a writable byte pointer and length, then increments each
  byte in place (`01 02 fe` becomes `02 03 ff`).

Flash the app and function image together:

```sh
make flash
```

`make flash` builds and writes all three function images and the app. To change
their locations, change the three `DATAFLASH_*_OFFSET` values in the Makefile,
then clean and rebuild so the app's function pointers are updated too:

```sh
make clean && make flash
```

`make flash-dataflash` rewrites all three function images without rewriting the
app; use it when the app already has matching function addresses. Keep each
offset in DataFlash, aligned to a 256-byte boundary, and leave room for its
image. Connect the CH582 through minichlink when running either flash target.
