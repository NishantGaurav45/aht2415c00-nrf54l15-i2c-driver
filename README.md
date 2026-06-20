# aht2415c00-nrf54l15-i2c-driver
Bare-metal I2C driver and nRF Connect SDK (Zephyr) firmware for reading the AHT2415C00 temperature/humidity probe on a Nordic nRF54L15.
# AHT2415C00 + nRF54L15 — temperature/humidity reader

Reads the AHT2415C00 probe over I2C and logs temperature (°C) and
relative humidity (%RH) every 2 seconds.

## Wiring (defaults used in this project)

| AHT2415C00 wire | nRF54L15-DK pin |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SCL | P1.11 |
| SDA | P1.12 |

If you wired it to different pins, edit the two `NRF_PSEL(...)` lines in
`boards/nrf54l15dk_nrf54l15_cpuapp.overlay` — nothing else needs to change.

I2C address used: `0x38` (default/fixed for this sensor family).

## Project layout

```
aht2415c_nrf54l15/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay
└── src/
    └── main.c
```

## Build

From an nRF Connect SDK toolchain environment (or VS Code's nRF Connect
extension terminal):

```bash
west build -b nrf54l15dk/nrf54l15/cpuapp -p
```

## Flash

Connect the DK via USB-C, then:

```bash
west flash
```

## View output

```bash
west espressif monitor   # if applicable
# or, more commonly for nRF54L15 DK:
nrfjprog --rtt           # or use the "Serial Terminal" / RTT viewer in
                          # nRF Connect for VS Code / nRF Connect for Desktop
```

Open the J-Link virtual COM port (115200 baud, 8N1) in any serial
terminal (PuTTY, minicom, the VS Code Serial Monitor, etc.) and you'll
see lines like:

```
[00:00:02.134,000] <inf> aht2415c: Temperature: 24.81 C  |  Humidity: 41.27 %RH
```

## Notes

- This sensor has no native Zephyr sensor driver, so `main.c` talks to it
  directly with `i2c_write_dt()` / `i2c_read_dt()` following the
  datasheet's command sequence (status read `0x71`, init `0xBE`, trigger
  `0xAC 0x33 0x00`).
- The board's I/O voltage must match the sensor — run both at 3.3V.
- If `i2c_write_read_dt`/`i2c_write_dt` calls fail (negative return
  codes), double check wiring and that pull-ups are present on SDA/SCL
  (the nRF54L15 DK does not always have I2C pull-ups by default on every
  pin — add 4.7kΩ pull-ups to 3V3 on both lines if reads fail).
