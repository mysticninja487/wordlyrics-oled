// Plain-assert tests; run with `make test`.
#include <cassert>
#include <cstdio>

#include "lyrics_core.h"

using namespace lyrics;

static void testParse() {
  const std::string lrc =
      "[ar:Someone]\n"
      "[ti:Title]\n"
      "[00:01.50]Hello big world\r\n"
      "[00:05.5]Second line\n"
      "[00:03][00:09.123]Repeated\n"
      "[00:12.00]\n"
      "[00:15.00]<00:15.10>Enhanced <00:15.50>tags\n"
      "not a lyric line\n";
  auto lines = parseLrc(lrc);
  assert(lines.size() == 6);
  assert(lines[0].startMs == 1500 && lines[0].text == "Hello big world");
  assert(lines[1].startMs == 3000 && lines[1].text == "Repeated");
  assert(lines[2].startMs == 5500 && lines[2].text == "Second line");
  assert(lines[3].startMs == 9123 && lines[3].text == "Repeated");
  assert(lines[4].startMs == 12000 && lines[4].text.empty());
  assert(lines[5].startMs == 15000 && lines[5].text == "Enhanced tags");
}

static void testTimelineBasics() {
  auto lines = parseLrc("[00:00.00]one two three\n[00:04.00]four\n");
  auto tl = buildTimeline(lines, 8000);
  // Words come out in order.
  assert(tl[0].word == "one" && tl[1].word == "two" && tl[2].word == "three");
  assert(tl[0].startMs == 0);
  // Events are contiguous, non-overlapping, never zero-length.
  for (size_t i = 0; i < tl.size(); ++i) {
    assert(tl[i].endMs > tl[i].startMs);
    if (i > 0) assert(tl[i].startMs == tl[i - 1].endMs);
  }
  // Timeline ends exactly at track end.
  assert(tl.back().endMs == 8000);
  // Last line "four" is held 1.5s then blanked until the track end.
  assert(tl[tl.size() - 2].word == "four");
  assert(tl[tl.size() - 1].word.empty());
}

static void testMinWordAndOrdering() {
  // 4 words in a 1s gap: can't honour 150ms min for long words, but must still be contiguous.
  auto lines = parseLrc("[00:00.00]a bb ccc dddd\n[00:01.00]next\n");
  auto tl = buildTimeline(lines, 3000);
  assert(tl[0].startMs == 0);
  uint32_t t = 0;
  for (auto& e : tl) {
    assert(e.startMs == t);
    t = e.endMs;
  }
  assert(t == 3000);
  // Longer words get at least as much time as shorter ones.
  assert((tl[3].endMs - tl[3].startMs) >= (tl[0].endMs - tl[0].startMs));
}

static void testLongGapDoesNotStretchWords() {
  auto lines = parseLrc("[00:00.00]hi there\n[00:30.00]end\n");
  auto tl = buildTimeline(lines, 40000);
  // "hi there" is ~8 chars * 90ms: nowhere near 30 seconds.
  assert(tl[1].word == "there");
  assert(tl[1].endMs < 5000);
  // After the hold, screen is blank until the next line.
  assert(tl[2].word.empty() && tl[2].endMs == 30000);
}

static void testFindEvent() {
  auto lines = parseLrc("[00:02.00]alpha beta\n[00:06.00]gamma\n");
  auto tl = buildTimeline(lines, 10000);
  assert(findEvent(tl, 0) == -1);       // before first line
  assert(findEvent(tl, 1999) == -1);
  int i = findEvent(tl, 2000);
  assert(i == 0 && tl[i].word == "alpha");
  i = findEvent(tl, 6000);
  assert(tl[i].word == "gamma");
  assert(findEvent(tl, 10000) == -1);   // at/after the end
  assert(findEvent(tl, 99999) == -1);
}

static void testUtf8AndEmpty() {
  assert(utf8Length("abc") == 3);
  assert(utf8Length("h\xC3\xA9llo") == 5);       // héllo
  assert(utf8Length("\xE0\xA6\xAC\xE0\xA6\xBE") == 2);  // two Bengali code points
  assert(parseLrc("").empty());
  assert(buildTimeline({}, 1000).empty());
  assert(findEvent({}, 0) == -1);
}

int main() {
  testParse();
  testTimelineBasics();
  testMinWordAndOrdering();
  testLongGapDoesNotStretchWords();
  testFindEvent();
  testUtf8AndEmpty();
  std::puts("all lyrics_core tests passed");
  return 0;
}
