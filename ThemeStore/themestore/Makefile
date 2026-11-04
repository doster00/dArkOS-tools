CFLAGS ?= -O2 -std=gnu99 -Wall -Wextra -Wno-unused-function
SRC = src/ui.c src/store.c

# Build on the console itself: make themestore   (needs gcc and libsdl2-dev)
themestore: src/main.c $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(shell sdl2-config --cflags --libs) -lm
	strip $@ 2>/dev/null || true

clean:
	rm -f themestore
