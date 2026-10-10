# Host-side tests for the lyrics core (no ESP32 needed).
CXX ?= g++
CORE = lib/LyricsCore/src

test: tests/host/test_core
	./tests/host/test_core

tests/host/test_core: $(CORE)/lyrics_core.cpp $(CORE)/lyrics_core.h tests/host/test_core.cpp
	$(CXX) -std=c++17 -Wall -Wextra -I$(CORE) $(CORE)/lyrics_core.cpp tests/host/test_core.cpp -o $@

clean:
	rm -f tests/host/test_core

.PHONY: test clean
