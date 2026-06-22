# STM32 Hello

Minimal bare-metal firmware for a STM32F1 Nucleo-64 style board.

What it does:

- Enables the GPIO clock.
- Configures the user LED pin as output.
- Sends `hello world` over `USART2`, which shows up on the board's virtual COM port.
- Blinks forever so you can see that flashing and reset work.

Default assumptions:

- User LED on `PA5`.
- Virtual COM port on `USART2` TX `PA2` at `115200 8N1`.
- Flash size `64 KB`.
- RAM window `4 KB`.

If your exact STM32F101 variant differs, edit `linker.ld` or the LED pin in `main.c`.

## Build

```bash
make
```

## Flash with OpenOCD

```bash
make flash
```

You can also flash the raw binary:

```bash
make flash-bin
```

## Serial output

Open the serial device with `115200` baud:

```bash
screen /dev/ttyACM0 115200
```

You should see `hello world` repeating on boot.

## Notes

- The project is freestanding and does not depend on CMSIS or a vendor HAL.
- It uses `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, and OpenOCD.
- OpenOCD defaults to `interface/stlink.cfg` and `target/stm32f1x.cfg`.
- If your board does not wire the ST-LINK VCP to `USART2`, adjust the UART pins in `main.c`.
