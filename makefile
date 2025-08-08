CC=gcc
CFLAGS=-g
TARGET=bin/todoc
INSTALL_PATH=~/bin/todoc

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

install: $(TARGET)
	cp $(TARGET) $(INSTALL_PATH)

clean:
	rm -f $(TARGET)
