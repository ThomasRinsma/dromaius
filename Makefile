CXX=g++ --std=c++11
CFLAGS=-g -O3 `sdl2-config --cflags`
LDFLAGS=`sdl2-config --libs` -lm

SOURCES=audio.cc cpu.cc graphics.cc input.cc main.cc memory.cc

.PHONY: all
all: gbemu

gbemu: $(subst .cc,.o,$(SOURCES))
	$(CXX) $^ -o $@ $(LDFLAGS)

%.o: src/%.cc
	$(CXX) -c $< $(CFLAGS)

clean:
	rm *.o gbemu