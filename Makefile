CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -DUNICODE -D_UNICODE
LDLIBS := -lcomctl32 -lshell32 -lgdi32

TARGET := build/playlist-manager.exe
OBJECTS := build/main.o build/playlist.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -municode $^ -o $@ $(LDLIBS)

build/main.o: src/main.cpp src/playlist.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/playlist.o: src/playlist.cpp src/playlist.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -f $(OBJECTS) $(TARGET)
