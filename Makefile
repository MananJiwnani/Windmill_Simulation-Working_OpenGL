CC      = gcc
TARGET  = windmill_sim
SRC     = main.c physics.c camera.c input.c rendering.c effects.c hud.c assets.c
HEADERS = state.h physics.h camera.h input.h rendering.h effects.h hud.h assets.h cgltf.h stb_image.h

CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lGL -lGLU -lGLEW -lglfw -lglut -lcglm -lm

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
