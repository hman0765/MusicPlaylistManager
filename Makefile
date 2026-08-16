CXX := g++
WINDRES := windres
TAGLIB_ROOT := D:/1001Repository/taglib
TAGLIB_LIB := $(TAGLIB_ROOT)/build-mingw/taglib/libtag.a
TAGLIB_INCLUDES := -I$(TAGLIB_ROOT)/taglib \
	-I$(TAGLIB_ROOT)/taglib/toolkit \
	-I$(TAGLIB_ROOT)/build-mingw

CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O0 \
	-DUNICODE -D_UNICODE -DTAGLIB_STATIC $(TAGLIB_INCLUDES)
LDLIBS := $(TAGLIB_LIB) -lcomctl32 -lcomdlg32 -lshell32 -lgdi32 -lole32 -luuid
LDFLAGS := -municode -mwindows -static

TARGET := build/playlist-manager.exe
OBJECTS := build/main.o build/playlist.o build/app_state.o build/drag_drop.o \
	build/ui_text.o \
	build/resource.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(TAGLIB_LIB)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

build/main.o: src/main.cpp src/app_state.h src/playlist.h src/drag_drop.h \
	src/ui_text.h \
	src/resource.h src/version.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/playlist.o: src/playlist.cpp src/playlist.h \
	$(TAGLIB_ROOT)/taglib/fileref.h \
	$(TAGLIB_ROOT)/taglib/tag.h \
	$(TAGLIB_ROOT)/taglib/audioproperties.h \
	$(TAGLIB_ROOT)/taglib/toolkit/tpropertymap.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/app_state.o: src/app_state.cpp src/app_state.h src/playlist.h src/ui_text.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/drag_drop.o: src/drag_drop.cpp src/drag_drop.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/ui_text.o: src/ui_text.cpp src/ui_text.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/resource.o: src/resource.rc src/resource.h src/version.h resources/app.ico | build
	$(WINDRES) -Isrc -I. $< -O coff -o $@

build:
	mkdir -p build

clean:
	powershell -NoProfile -Command "$$files = @('build/main.o', 'build/playlist.o', 'build/app_state.o', 'build/drag_drop.o', 'build/ui_text.o', 'build/resource.o', 'build/playlist-manager.exe'); foreach ($$file in $$files) { if (Test-Path -LiteralPath $$file) { Remove-Item -Force -LiteralPath $$file } }"
