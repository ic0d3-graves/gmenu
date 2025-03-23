CC = gcc
CFLAGS = -Wall -O2
XFT_CFLAGS = $(shell pkg-config --cflags xft)
LIBS = -lX11 -lXrender -lXft
TARGET = gmenu

all: $(TARGET)

$(TARGET): gmenu.c
	$(CC) $(CFLAGS) $(XFT_CFLAGS) -o $(TARGET) gmenu.c $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/bin/

.PHONY: all clean install