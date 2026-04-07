# ============================================================
#  Farm Windmill Sim — Makefile
#  Compiler: gcc
#  Deps:     GLEW, GLFW3, cglm, OpenGL, GLU, math
#  Headers:  cgltf.h, stb_image.h  (single-header, in project root)
# ============================================================

CC      = gcc
TARGET  = windmill_sim
SRC     = main.c

# Flags
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lGL -lGLU -lGLEW -lglfw -lcglm -lm

all: $(TARGET)

$(TARGET): $(SRC) cgltf.h stb_image.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
