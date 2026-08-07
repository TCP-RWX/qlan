CC=gcc
LDFLAGS = -lm

TARGET = qlan
SRC = src/qlan.c

PREFIX = /bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

debug: CFLAGS += -g -O0 -Wall -Wextra -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) $(PREFIX)/$(TARGET)
	sudo chmod 755 $(PREFIX)/$(TARGET)

uninstall:
	sudo rm -f $(PREFIX)/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install uninstall clean
