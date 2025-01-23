CFLAGS  := $(shell pkg-config --cflags gtk4)
LDFLAGS := $(shell pkg-config --libs gtk4)

all: inventory

inventory: inventory.c
	gcc $(CFLAGS) -l sqlite3 -o inventory inventory.c $(LDFLAGS)

clean:
	rm -f inventory
