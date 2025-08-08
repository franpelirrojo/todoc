CC=gcc
CFLAGS=-g
TARGET=bin/todoc
INSTALL_PATH=~/bin/todoc
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

$(TARGET): $(OBJ)
	gcc -o $@ $?

obj/%.o: src/%.c
	gcc -c $< -o $@ -Iinclude

install: $(TARGET)
	cp $(TARGET) $(INSTALL_PATH)

clean:
	rm -f obj/*.o
	rm -f bin/*
