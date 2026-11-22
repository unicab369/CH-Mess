# CH582 examples

## Debug pins

For the WCH debug interface:
- **SWDIO:** PB14 (TIO)
- **SWCLK:** PB15 (TCK)

## Enable the hardware reset button

Some CH582 boards have the external `RST#` input disabled in their option
bytes, so pressing the board's reset button does not reset the chip. With a
WCH-Link connected, build MiniCHLink from the repository root:

```sh
make -C ch32fun/minichlink
```

Then enable the reset input using MiniCHLink from its directory:
```sh
./minichlink -B -d
```


To disable the hardware reset input and return PB23 to GPIO use, run:
```sh
./minichlink -B -D
```

Verify it with:
```sh
./minichlink -i
```

On the CH582, the external `RST#` input is shared with **PB23**. Enabling the
reset input reserves PB23 for reset, so do not use PB23 as a GPIO at the same
time. For example, move the ST7735 DC signal off PB23 when hardware reset is
enabled.

git clone https://github.com/cnlohr/ch570funprog --recursive

cd ch570funprog
# Plug it in
make flash_fresh
