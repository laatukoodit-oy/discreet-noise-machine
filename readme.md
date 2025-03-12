# Title

## Usage

The project `Makefile` expects a `make.conf` in the project root directory with the following variables:

- PROGRAMMER
- BAUD\_RATE
- PORT
- MCU
- F\_CPU

An example make.conf for an Arduino UNO board on port COM4:

```make
PROGRAMMER = arduino
BAUD_RATE = 115200
PORT = COM4

MCU = atmega328p
F_CPU = 16000000UL
```
