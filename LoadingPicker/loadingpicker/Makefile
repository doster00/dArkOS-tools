CFLAGS ?= -O2 -std=gnu99 -Wall -Wextra -Wno-unused-function
loadingpicker: src/main.c src/ui.c
	$(CC) $(CFLAGS) $^ -o $@ $(shell sdl2-config --cflags --libs) -lm
clean:
	rm -f loadingpicker
