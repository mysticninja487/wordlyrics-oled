# wordlyrics-oled

Show song lyrics **one word at a time** on a small OLED, driven by an ESP32.

The words are centered and auto-sized to fit the screen, with a thin progress bar along the bottom edge. It started as a "holographic lyrics" idea, minus the glass cube: the display just faces you directly.

**Status: v0.1, offline demo.** It loops a built-in sample track (original lyrics written for this repo) so you can check your wiring and the look of the display. WiFi lyric fetching and real playback sync are on the roadmap below.

## Hardware

| Part | Notes | Approx. price (BDT) |
|------|-------|---------------------|
| ESP32 DevKit V1 (30-pin) | or an ESP32-C3 board, e.g. ESP32-C3 Pro Mini at ~520 | ~800-1,100 (estimate) |
| 0.96" 128x64 I2C OLED (SSD1306) | I2C address 0x3C | ~300 |
| Breadboard (half size) | | ~80 |
| Jumper wires, male-female (x4 needed) | | ~3 each |
| USB data cable | Micro-USB for DevKit V1, USB-C for most C3 boards | ~80-150 |

Prices are from ElectronicsBD in October 2026, except the DevKit V1 and the cable, which are estimates. They don't include delivery.

### Wiring (ESP32 DevKit V1)

| OLED | ESP32 |
|------|-------|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

For ESP32-C3 boards the I2C pins differ between board makers. Check your board's pinout and edit `OLED_SDA` / `OLED_SCL` under `[env:esp32c3]` in `platformio.ini` (the defaults there are GPIO8/GPIO9).

## Build and flash

Install [PlatformIO](https://platformio.org/install) (the VS Code extension or the CLI), then:

```sh
pio run -e esp32dev -t upload   # classic ESP32 DevKit
pio run -e esp32c3 -t upload    # ESP32-C3 boards
pio device monitor              # serial log
```

The firmware was **not compiled or run on real hardware** by its author before the first commit. If the build breaks on your machine, please open an issue with the error output.

## Tests (no hardware needed)

The lyric parsing and word timing live in `lib/LyricsCore` as plain C++, so they run on your PC:

```sh
make test
```

## How the word timing works

Synced lyrics (LRC format) only give a timestamp per *line*, not per word. So each line's words are spread across the time until the next line starts:

1. A line's duration is the gap to the next line's timestamp.
2. Words are weighted by character count, at roughly 90 ms per character, so a long gap does not stretch a two-word line across 30 seconds.
3. Every word gets at least 150 ms where the line allows it.
4. The last word of a line is held for 1.5 s, then the screen goes blank until the next line.

These numbers live in `TimingConfig` in `lib/LyricsCore/src/lyrics_core.h`.

## Roadmap

- [x] v0.1: LRC parser, word timeline, OLED demo, host tests
- [ ] v0.2: fetch synced lyrics over WiFi from [LRCLIB](https://lrclib.net) by track name, artist and duration
- [ ] v0.3: real playback sync (Spotify "currently playing" API, or a small companion script on your PC/phone)
- [ ] v0.4: animations: slide, typewriter, scale "pop", glitch
- [ ] v0.5: optional color SPI display (e.g. 2.4" ST7789) for gradients and bigger effects
- [ ] Button for pause / skip

## A note on lyrics

Lyrics are copyrighted. This repo only ships original sample text. For real songs, fetch lyrics at runtime from a service rather than committing them here.

## License

MIT, see [LICENSE](LICENSE).
