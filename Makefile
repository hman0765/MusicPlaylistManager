CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -DUNICODE -D_UNICODE
LDLIBS := -lcomctl32 -lcomdlg32 -lshell32 -lgdi32 -lole32 -lpropsys -luuid
LDFLAGS := -municode -mwindows -static

TARGET := build/playlist-manager.exe
OBJECTS := build/main.o build/playlist.o build/app_state.o build/drag_drop.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/main.o: src/main.cpp src/playlist.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/playlist.o: src/playlist.cpp src/playlist.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/app_state.o: src/app_state.cpp src/app_state.h src/playlist.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/drag_drop.o: src/drag_drop.cpp src/drag_drop.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	powershell -NoProfile -Command "$$files = @('build/main.o', 'build/playlist.o', 'build/app_state.o', 'build/drag_drop.o', 'build/playlist-manager.exe'); foreach ($$file in $$files) { if (Test-Path -LiteralPath $$file) { Remove-Item -Force -LiteralPath $$file } }"
