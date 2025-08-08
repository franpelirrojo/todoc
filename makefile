CC=gcc
CFLAGS=-g
TARGET=bin/todoc
INSTALL_PATH=~/bin/todoc

default: $(TARGET)

install:
	cp $(TARGET) $(INSTALL_PATH)

clean:
	rm -f $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

