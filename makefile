CC=gcc
CFLAGS = -g -Wall -Wextra -Wimplicit-fallthrough -Iinclude

TARGET=bin/td
INSTALL_PATH=~/bin/td

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

run: clean $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $?

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: run 
	cp $(TARGET) $(INSTALL_PATH)

clean:
	rm -f obj/*.o
	rm -f bin/*

test:
	./test.sh
