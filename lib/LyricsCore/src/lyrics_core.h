// Pure C++ (no Arduino dependencies) so it can be unit-tested on a PC.
//
// Pipeline:  LRC text --parseLrc--> lines --buildTimeline--> word events
//            playback position (ms) --findEvent--> which word to show
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lyrics {

// One synced lyric line, e.g. "[01:23.45]some words here".
struct Line {
  uint32_t startMs;
  std::string text;  // may be empty (instrumental gap)
};

// One thing to show on screen for a time window. Empty word = blank screen.
struct WordEvent {
  uint32_t startMs;
  uint32_t endMs;  // exclusive
  std::string word;
};

struct TimingConfig {
  uint32_t minWordMs = 150;     // never flash a word shorter than this (if the line allows)
  uint32_t msPerChar = 90;      // rough singing speed used to spread words in a line
  uint32_t holdLastMs = 1500;   // keep the last word up this long, then blank until next line
  uint32_t lastLineMs = 4000;   // duration assumed for the final line if track length unknown
};

// Parses LRC text. Handles [mm:ss.xx] / [mm:ss.xxx] / [mm:ss] stamps, several
// stamps on one line, ignores metadata tags like [ar:...], strips enhanced
// <mm:ss.xx> word tags. Result is sorted by time.
std::vector<Line> parseLrc(const std::string& lrc);

// Spreads each line's words across the time until the next line starts.
// trackEndMs (optional, 0 = unknown) bounds the final line.
std::vector<WordEvent> buildTimeline(const std::vector<Line>& lines,
                                     uint32_t trackEndMs = 0,
                                     const TimingConfig& cfg = TimingConfig());

// Index of the event active at posMs, or -1 if none (before the first line / after the last).
int findEvent(const std::vector<WordEvent>& timeline, uint32_t posMs);

// Number of Unicode code points in a UTF-8 string.
size_t utf8Length(const std::string& s);

}  // namespace lyrics
