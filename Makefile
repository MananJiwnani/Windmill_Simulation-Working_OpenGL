CC      = gcc
TARGET  = windmill_sim
SRC     = main.c

CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lGL -lGLU -lGLEW -lglfw -lglut -lcglm -lm

all: $(TARGET)

$(TARGET): $(SRC) cgltf.h stb_image.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
