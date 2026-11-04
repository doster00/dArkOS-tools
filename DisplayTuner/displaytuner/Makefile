CFLAGS ?= -O2 -std=gnu99 -Wall -Wextra -Wno-unused-function
SRC = src/ui.c src/lut.c src/scene.c src/studio.c

# Build on the console itself: make displaytuner   (needs gcc and libsdl2-dev)
displaytuner: src/main.c $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(shell sdl2-config --cflags --libs) -lm -ldl
	strip $@ 2>/dev/null || true

clean:
	rm -f displaytuner
