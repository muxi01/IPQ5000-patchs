CC      ?= gcc
LD      ?= ld
STRIP   ?= strip
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -static
TARGET  := navigation

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c nav_template.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c nav_template.o
	$(STRIP) $@

nav_template.o: nav_template.html
	$(LD) -r -b binary nav_template.html -o $@

clean:
	rm -f $(TARGET) nav_template.o