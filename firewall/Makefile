CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
TARGET  := firewall
OBJS    := fw_html.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ main.c $(OBJS)

fw_html.o: fw.html
	ld -r -b binary fw.html -o $@

clean:
	rm -f $(TARGET) $(OBJS)