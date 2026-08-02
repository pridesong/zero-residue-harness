CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
TARGET   = audit

all: $(TARGET)

$(TARGET): audit.c
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
