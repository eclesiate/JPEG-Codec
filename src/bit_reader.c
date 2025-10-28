#include "../inc/bit_reader.h"
#include <stdio.h>

void init_bit_reader(Bit_Reader* reader, FILE* file) {
    reader->file = file;
    reader->next_byte = 0;
    reader->next_bit = 0;
}

bool has_bits(Bit_Reader* reader) {
    return !feof(reader->file);
}

byte read_byte(Bit_Reader* reader) {
    reader->next_bit = 0;
    return fgetc(reader->file);
}

uint read_word(Bit_Reader* reader) {
    reader->next_bit = 0;
    uint high = fgetc(reader->file);
    uint low = fgetc(reader->file);
    return (high << 8) | low;
}
// Gpt generated to give next bit, respects big-endian and skips padding bits
int read_bit(Bit_Reader* reader) {
    // If we're at the start of a new byte, read it
    if (reader->next_bit == 0) {
        if (!has_bits(reader)) {
            return -1;
        }
        
        reader->next_byte = fgetc(reader->file);
        
        // Handle byte stuffing: 0xFF followed by 0x00 means literal 0xFF
        // Also handle restart markers (0xFF followed by 0xD0-0xD7)
        while (reader->next_byte == 0xFF) {
            if (!has_bits(reader)) {
                return -1;
            }
            
            int marker = fgetc(reader->file);
            
            // Skip multiple 0xFF in a row
            while (marker == 0xFF) {
                if (!has_bits(reader)) {
                    return -1;
                }
                marker = fgetc(reader->file);
            }
            
            // Literal 0xFF (byte stuffing)
            if (marker == 0x00) {
                break;
            }
            // Restart markers - reset and continue
            else if (marker >= RST0 && marker <= RST7) {
                reader->next_byte = fgetc(reader->file);
            }
            else {
                fprintf(stderr, "Error - Invalid marker during bitstream: 0x%02x\n", marker);
                return -1;
            }
        }
    }
    
    // Extract the bit at position (7 - next_bit)
    int bit = (reader->next_byte >> (7 - reader->next_bit)) & 1;
    reader->next_bit = (reader->next_bit + 1) % 8;
    
    return bit;
}

int read_bits(Bit_Reader* reader, uint length) {
    if (length == 0) {
        return 0;
    }
    
    if (length > 32) {
        fprintf(stderr, "Error - Cannot read more than 32 bits at once\n");
        return -1;
    }
    
    uint bits = 0;
    for (uint i = 0; i < length; ++i) {
        int bit = read_bit(reader);
        if (bit == -1) {
            return -1;
        }
        bits = (bits << 1) | bit;
    }
    
    return bits;
}
