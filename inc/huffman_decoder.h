#ifndef HUFFMAN_DECODER_H
#define HUFFMAN_DECODER_H

#include "jpeg.h"
#include "bit_reader.h"

/// @brief 
/// @param table 
void generate_huffman_codes(Huffman_Table* table);

/// @brief get next huffman symbol from bistream
/// @param reader 
/// @param table 
/// @return -1 on error
int get_next_symbol(Bit_Reader* reader, const Huffman_Table* table);


/// @brief decode an 8x8 block component for y, cb, or cr
/// @param image 
/// @param reader 
/// @param component 
/// @param previous_dc 
/// @param skips number of blocks to skip TODO implement for progressive
/// @param dc_table 
/// @param ac_table 
/// @return true on success
bool decode_block_component(
    const Image* image,
    Bit_Reader* reader,
    int* component,          
    int* previous_dc,        
    const Huffman_Table* dc_table,
    const Huffman_Table* ac_table
);

// entry function to decode all huffman data
void decode_huffman_data(Image* image, Bit_Reader* reader);

#endif