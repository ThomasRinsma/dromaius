CC=gcc
CFLAGS=-g -O3 `sdl-config --cflags`
LFLAGS=`sdl-config --libs`

all:
	$(CC) $(CFLAGS) *.c -o gbemu $(LFLAGS)

clean:
	rm gbemu
