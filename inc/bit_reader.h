#ifndef BIT_READER_H
#define BIT_READER_H

#include <stdio.h>
#include <stdbool.h>
#include "jpeg.h"

typedef struct Bit_Reader Bit_Reader;
struct Bit_Reader {
    FILE* file;
    byte next_byte;
    byte next_bit;  // Position in curr byte (0-7)
};

// Initialize a bit reader with an open file
void init_bit_reader(Bit_Reader* reader, FILE* file);

// Check if there are more bits to read
bool has_bits(Bit_Reader* reader);

// Read a single byte (aligned to byte boundary)
byte read_byte(Bit_Reader* reader);

// Read a 16-bit word (big-endian, aligned to byte boundary)
uint read_word(Bit_Reader* reader);

// Read a single bit (0 or 1), returns -1 on error/EOF
int read_bit(Bit_Reader* reader);

// Read multiple bits (up to 32 bits)
// Returns -1 on error, otherwise returns the bits as an unsigned int
int read_bits(Bit_Reader* reader, uint length);

// Align to the next byte boundary (skip remaining bits in current byte)
void align_reader(Bit_Reader* reader);

#endif