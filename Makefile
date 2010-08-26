CC       = gcc
SRC      = lolock.c
HEADERS	 = font.h
OBJ      = $(SRC:.c=.o)
EXEC     = lolock
LDFLAGS  = `sdl-config --libs` -lSDL_ttf -lpam
CFLAGS   = `sdl-config --cflags` -Wall -Wextra

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: all clean distclean

clean:
	rm -rf $(OBJ)

distclean: clean
	rm -rf $(EXEC)
