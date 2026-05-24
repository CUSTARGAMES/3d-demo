CC = gcc
CFLAGS = -Wall -O2
LIBS = -lSDL2 -lm

all: raycaster

raycaster: main.c raycast.c
	$(CC) $(CFLAGS) -o raycaster main.c raycast.c $(LIBS)

clean:
	rm -f raycaster raycaster.exe

.PHONY: all clean
