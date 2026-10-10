// Word-by-word lyrics on a 128x64 SSD1306 OLED (ESP32).
// v0.1: offline demo. Plays the built-in sample LRC on a loop from boot time.
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include <string>
#include <vector>

#include "lyrics_core.h"
#include "sample_lyrics.h"

#ifndef OLED_SDA
#define OLED_SDA 21
#endif
#ifndef OLED_SCL
#define OLED_SCL 22
#endif

static const int kScreenW = 128;
static const int kTextAreaH = 60;  // bottom 4px reserved for the progress bar
static const int kMargin = 2;

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Largest first; each word uses the biggest font it fits in.
static const uint8_t* const kFonts[] = {
    u8g2_font_logisoso32_tf, u8g2_font_logisoso24_tf, u8g2_font_logisoso20_tf,
    u8g2_font_logisoso16_tf, u8g2_font_helvB10_tf,
};
static const size_t kFontCount = sizeof(kFonts) / sizeof(kFonts[0]);

static std::vector<lyrics::WordEvent> timeline;
static uint32_t startMs = 0;

static void drawCenteredWord(const std::string& word) {
  size_t chosen = kFontCount - 1;
  for (size_t i = 0; i < kFontCount; ++i) {
    u8g2.setFont(kFonts[i]);
    if (u8g2.getUTF8Width(word.c_str()) <= kScreenW - 2 * kMargin) {
      chosen = i;
      break;
    }
  }
  u8g2.setFont(kFonts[chosen]);
  const int w = u8g2.getUTF8Width(word.c_str());
  const int asc = u8g2.getAscent();
  const int desc = u8g2.getDescent();  // negative
  const int x = (kScreenW - w) / 2;
  const int top = (kTextAreaH - (asc - desc)) / 2;
  u8g2.drawUTF8(x < 0 ? 0 : x, top + asc, word.c_str());
}

void setup() {
  Serial.begin(115200);
  Wire.setPins(OLED_SDA, OLED_SCL);
  u8g2.setBusClock(400000);
  u8g2.begin();

  auto lines = lyrics::parseLrc(SAMPLE_LRC);
  timeline = lyrics::buildTimeline(lines, SAMPLE_TRACK_MS);
  Serial.printf("lines=%u words/events=%u\n", (unsigned)lines.size(), (unsigned)timeline.size());

  startMs = millis();
}

void loop() {
  const uint32_t loopLen = SAMPLE_TRACK_MS + 2000;  // short pause before repeating
  const uint32_t pos = (millis() - startMs) % loopLen;
  const int idx = lyrics::findEvent(timeline, pos);

  u8g2.clearBuffer();
  if (idx >= 0 && !timeline[idx].word.empty()) {
    drawCenteredWord(timeline[idx].word);
  }
  const uint32_t clamped = pos < SAMPLE_TRACK_MS ? pos : SAMPLE_TRACK_MS;
  u8g2.drawHLine(0, 63, (uint32_t)((uint64_t)clamped * kScreenW / SAMPLE_TRACK_MS));
  u8g2.sendBuffer();
}
