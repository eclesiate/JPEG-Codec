.PHONY: all clean

CLFAGS= -Wall -Wextra -std=c11 -pedantic -fsanitize=address -Wswitch=enum
LIBS=

all: bin/decoder #bin/encoder

clean:
	@rm -rf bin

bin/decoder: src/decoder.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/decoder src/decoder.c $(LIBS)

# bin/encoder: src/encoder.c
# 	@mkdir -p bin
# 	$(CC) $(CFLAGS) -o bin/encoder src/encoder.c $(LIBS)


	