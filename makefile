INC = include
OBJ = obj
SRC = src
EXE = game.bin

CC = gcc
CFLAGS = -w -ggdb -I$(INC)
LIBS = -lGLEW -lGLU -lGL -lSDL2 -lcglm -lz -lm

OBJFILES := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(shell find $(SRC) -name '*.c'))
OBJDIRS := $(patsubst $(SRC)%, $(OBJ)%, $(shell find $(SRC) -type d))
CLEANDIRS := $(addsuffix /.clean, $(OBJDIRS));

# Explicit targets
.PHONY: all clean
.DEFAULT_GOAL := all

# Build executable
all: $(EXE)
$(EXE): $(OBJFILES) | $(OBJDIRS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

# Compile object files
$(OBJ)/%.o: $(SRC)/%.c | $(OBJDIRS)
	$(CC) -c -o $@ $(SRC)/$*.c $(CFLAGS)

# Create obj directory tree if it doesn't exist
$(OBJDIRS):
	mkdir -p $@

# Delete previously built files
clean: $(CLEANDIRS)
	rm -f $(EXE)
%.clean:
	rm -f $**.o
