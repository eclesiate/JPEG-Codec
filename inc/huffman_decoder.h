#ifndef HUFFMAN_DECODER_H
#define HUFFMAN_DECODER_H

#include "jpeg.h"
#include "bit_reader.h"

// Generate Huffman codes from a Huffman table's symbol counts
void generate_huffman_codes(Huffman_Table* table);

// Get the next Huffman symbol from the bitstream
// Returns -1 on error
int get_next_symbol(Bit_Reader* reader, const Huffman_Table* table);

// Decode a single 8x8 block component (Y, Cb, or Cr)
// Returns true on success, false on error
bool decode_block_component(
    const Image* image,
    Bit_Reader* reader,
    int* component,           // Output: 64 DCT coefficients
    int* previous_dc,         // Input/Output: Previous DC value for this component
    uint* skips,              // Input/Output: Number of blocks to skip (progressive only)
    const Huffman_Table* dc_table,
    const Huffman_Table* ac_table
);

// Decode all Huffman data for the current scan
void decode_huffman_data(Image* image, Bit_Reader* reader);

#endif