CXX = g++ --std=c++11
CFLAGS =-g -O0 -I libs/imgui -I libs/gl3w `sdl2-config --cflags` -Wno-pmf-conversions
LDFLAGS =`sdl2-config --libs` -lm -lGL -ldl

SOURCES = audio.cc cpu.cc graphics.cc gui.cc input.cc main.cc memory.cc dromaius.cc
SOURCES += imgui_impl_sdl.cc imgui_impl_opengl3.cc
LIBSOURCES = libs/imgui/imgui.cpp libs/imgui/imgui_widgets.cpp libs/imgui/imgui_demo.cpp
LIBSOURCES += libs/imgui/imgui_draw.cpp libs/gl3w/GL/gl3w.c


.PHONY: all
all: dromaius

dromaius: $(subst .cc,.o,$(SOURCES))
	$(CXX) $^ $(LIBSOURCES) -o $@ $(CFLAGS) $(LDFLAGS)

%.o: src/%.cc
	$(CXX) -c $< $(CFLAGS)

clean:
	rm *.o dromaius
