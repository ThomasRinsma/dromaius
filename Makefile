CC=gcc
CFLAGS=-g -O3 `sdl2-config --cflags`
LFLAGS=`sdl2-config --libs`

all:
	$(CC) $(CFLAGS) *.c -o gbemu $(LFLAGS)

clean:
	rm gbemu
