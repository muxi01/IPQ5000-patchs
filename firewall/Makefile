CC      ?= gcc
LD      ?= ld
STRIP   ?= strip
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -static
TARGET  := firewall
OBJS    := fw_html.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c $(OBJS)
	$(STRIP) $@

fw_html.o: fw.html
	$(LD) -r -b binary fw.html -o $@

clean:
	rm -f $(TARGET) $(OBJS)