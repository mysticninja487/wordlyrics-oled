#include "lyrics_core.h"

#include <algorithm>
#include <cctype>

namespace lyrics {

size_t utf8Length(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) {
    if ((c & 0xC0) != 0x80) ++n;  // count everything except continuation bytes
  }
  return n;
}

namespace {

bool allDigits(const std::string& s) {
  if (s.empty()) return false;
  for (unsigned char c : s) {
    if (!std::isdigit(c)) return false;
  }
  return true;
}

// Parses the inside of "[...]" as a timestamp. Returns false for metadata tags.
bool parseStamp(const std::string& tag, uint32_t& outMs) {
  size_t colon = tag.find(':');
  if (colon == std::string::npos) return false;
  std::string minStr = tag.substr(0, colon);
  std::string rest = tag.substr(colon + 1);
  if (!allDigits(minStr)) return false;

  std::string secStr = rest;
  std::string fracStr;
  size_t dot = rest.find('.');
  if (dot != std::string::npos) {
    secStr = rest.substr(0, dot);
    fracStr = rest.substr(dot + 1);
    if (!allDigits(fracStr)) return false;
  }
  if (!allDigits(secStr)) return false;

  uint32_t ms = 0;
  if (!fracStr.empty()) {
    fracStr = fracStr.substr(0, 3);
    while (fracStr.size() < 3) fracStr += '0';
    ms = static_cast<uint32_t>(std::stoul(fracStr));
  }
  outMs = static_cast<uint32_t>(std::stoul(minStr)) * 60000u +
          static_cast<uint32_t>(std::stoul(secStr)) * 1000u + ms;
  return true;
}

std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

// Removes enhanced-LRC word tags like <00:12.34>.
std::string stripAngleTags(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  bool inTag = false;
  for (char c : s) {
    if (c == '<') {
      inTag = true;
    } else if (c == '>' && inTag) {
      inTag = false;
    } else if (!inTag) {
      out += c;
    }
  }
  return out;
}

std::vector<std::string> splitWords(const std::string& s) {
  std::vector<std::string> words;
  std::string cur;
  for (char c : s) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!cur.empty()) {
        words.push_back(cur);
        cur.clear();
      }
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) words.push_back(cur);
  return words;
}

}  // namespace

std::vector<Line> parseLrc(const std::string& lrc) {
  std::vector<Line> out;
  size_t pos = 0;
  while (pos <= lrc.size()) {
    size_t nl = lrc.find('\n', pos);
    std::string raw = lrc.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = (nl == std::string::npos) ? lrc.size() + 1 : nl + 1;

    if (!raw.empty() && raw.back() == '\r') raw.pop_back();

    // Read leading [tag] groups.
    std::vector<uint32_t> stamps;
    size_t i = 0;
    while (i < raw.size() && raw[i] == '[') {
      size_t close = raw.find(']', i);
      if (close == std::string::npos) break;
      uint32_t ms;
      if (parseStamp(raw.substr(i + 1, close - i - 1), ms)) stamps.push_back(ms);
      i = close + 1;
    }
    if (stamps.empty()) continue;  // metadata or junk line

    std::string text = trim(stripAngleTags(raw.substr(i)));
    for (uint32_t t : stamps) out.push_back(Line{t, text});
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Line& a, const Line& b) { return a.startMs < b.startMs; });
  return out;
}

std::vector<WordEvent> buildTimeline(const std::vector<Line>& lines, uint32_t trackEndMs,
                                     const TimingConfig& cfg) {
  std::vector<WordEvent> tl;
  for (size_t li = 0; li < lines.size(); ++li) {
    const uint32_t start = lines[li].startMs;
    uint32_t end;
    if (li + 1 < lines.size()) {
      end = lines[li + 1].startMs;
    } else {
      end = (trackEndMs > start) ? trackEndMs : start + cfg.lastLineMs;
    }
    if (end <= start) continue;  // duplicate timestamp: nothing to show

    const std::vector<std::string> words = splitWords(lines[li].text);
    if (words.empty()) {
      tl.push_back(WordEvent{start, end, ""});
      continue;
    }

    const uint32_t n = static_cast<uint32_t>(words.size());
    std::vector<uint32_t> weight(n);
    uint64_t totalW = 0;
    for (uint32_t w = 0; w < n; ++w) {
      size_t len = utf8Length(words[w]);
      weight[w] = static_cast<uint32_t>(len == 0 ? 1 : len);
      totalW += weight[w];
    }

    const uint32_t gap = end - start;
    uint32_t sung = static_cast<uint32_t>(
        std::min<uint64_t>(gap, std::max<uint64_t>(totalW * cfg.msPerChar,
                                                   static_cast<uint64_t>(n) * cfg.minWordMs)));

    std::vector<uint32_t> dur(n);
    if (static_cast<uint64_t>(n) * cfg.minWordMs >= sung) {
      // Too tight for minimums: split evenly.
      for (uint32_t w = 0; w < n; ++w) dur[w] = sung / n;
    } else {
      const uint64_t extra = sung - static_cast<uint64_t>(n) * cfg.minWordMs;
      for (uint32_t w = 0; w < n; ++w) {
        dur[w] = cfg.minWordMs + static_cast<uint32_t>(extra * weight[w] / totalW);
      }
    }
    // Put any rounding remainder on the last word so durations sum to `sung`.
    uint32_t used = 0;
    for (uint32_t w = 0; w + 1 < n; ++w) used += dur[w];
    dur[n - 1] = sung > used ? sung - used : 0;

    uint32_t t = start;
    for (uint32_t w = 0; w < n; ++w) {
      uint32_t wEnd = t + dur[w];
      if (w + 1 == n) {
        // Hold the last word, then blank out if the gap is long.
        uint32_t heldEnd = std::min(end, wEnd + cfg.holdLastMs);
        tl.push_back(WordEvent{t, heldEnd, words[w]});
        if (heldEnd < end) tl.push_back(WordEvent{heldEnd, end, ""});
      } else {
        tl.push_back(WordEvent{t, wEnd, words[w]});
      }
      t = wEnd;
    }
  }
  return tl;
}

int findEvent(const std::vector<WordEvent>& timeline, uint32_t posMs) {
  // Last event whose start <= posMs.
  auto it = std::upper_bound(timeline.begin(), timeline.end(), posMs,
                             [](uint32_t p, const WordEvent& e) { return p < e.startMs; });
  if (it == timeline.begin()) return -1;
  int idx = static_cast<int>(std::distance(timeline.begin(), it)) - 1;
  if (posMs >= timeline[idx].endMs) return -1;
  return idx;
}

}  // namespace lyrics
