#include "../inc/huffman_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generate_huffman_codes(Huffman_Table* table) {
    uint code = 0;
    for (uint i = 0; i < 16; ++i) {
        for (uint j = table->offsets[i]; j < table->offsets[i + 1]; ++j) {
            table->codes[j] = code;
            code += 1;
        }
        code <<= 1;
    }
}

int get_next_symbol(Bit_Reader* reader, const Huffman_Table* table) {
    uint current_code = 0;
    
    // Try codes of increasing length (1-16 bits)
    for (uint i = 0; i < 16; ++i) {
        int bit = read_bit(reader);
        if (bit == -1) {
            return -1;
        }
        
        current_code = (current_code << 1) | bit;
        
        // Check if current_code matches any symbol at this bit length
        for (uint j = table->offsets[i]; j < table->offsets[i + 1]; ++j) {
            if (current_code == table->codes[j]) {
                return table->symbols[j];
            }
        }
    }
    
    fprintf(stderr, "Error - Invalid Huffman code\n");
    return -1;
}
/*
// Helper function to calculate bit length of a value
static uint bit_length(int value) {
    if (value < 0) {
        value = -value;
    }
    
    uint length = 0;
    while (value > 0) {
        value >>= 1;
        length++;
    }
    return length;
}
*/
bool decode_block_component(
    const Image* image,
    Bit_Reader* reader,
    int* component,
    int* previous_dc,
    uint* skips,
    const Huffman_Table* dc_table,
    const Huffman_Table* ac_table)
{
    // Initialize component to all zeros
    memset(component, 0, 64 * sizeof(int));
    
    if (image->frame_type == SOF0) {
        // Baseline JPEG decoding
        
        // Decode DC coefficient
        int length = get_next_symbol(reader, dc_table);
        if (length == -1) {
            fprintf(stderr, "Error - Invalid DC value\n");
            return false;
        }
        if (length > 11) {
            fprintf(stderr, "Error - DC coefficient length > 11\n");
            return false;
        }
        
        int coeff = 0;
        if (length > 0) {
            coeff = read_bits(reader, length);
            if (coeff == -1) {
                fprintf(stderr, "Error - Invalid DC value\n");
                return false;
            }
            // If high bit is 0, value is negative
            if (coeff < (1 << (length - 1))) {
                coeff -= (1 << length) - 1;
            }
        }
        
        component[0] = coeff + *previous_dc;
        *previous_dc = component[0];
        
        // Decode AC coefficients
        for (uint i = 1; i < 64; ++i) {
            int symbol = get_next_symbol(reader, ac_table);
            if (symbol == -1) {
                fprintf(stderr, "Error - Invalid AC value\n");
                return false;
            }
            
            // Symbol 0x00 means rest of block is zero
            if (symbol == 0x00) {
                return true;
            }
            
            // Extract run-length and coefficient length
            byte num_zeroes = symbol >> 4;
            byte coeff_length = symbol & 0x0F;
            
            // Special case: 0xF0 means 16 zeroes
            if (symbol == 0xF0) {
                i += 15;
                continue;
            }
            
            if (i + num_zeroes >= 64) {
                fprintf(stderr, "Error - Zero run-length exceeded block\n");
                return false;
            }
            
            i += num_zeroes;
            
            if (coeff_length > 10) {
                fprintf(stderr, "Error - AC coefficient length > 10\n");
                return false;
            }
            
            coeff = read_bits(reader, coeff_length);
            if (coeff == -1) {
                fprintf(stderr, "Error - Invalid AC value\n");
                return false;
            }
            
            // If high bit is 0, value is negative
            if (coeff < (1 << (coeff_length - 1))) {
                coeff -= (1 << coeff_length) - 1;
            }
            
            component[zig_zag_map[i]] = coeff;
        }
        
        return true;
    }
    else {
        // Progressive JPEG decoding (SOF2)
        // This is complex - for now, we'll implement baseline only
        fprintf(stderr, "Error - Progressive JPEG decoding not yet implemented\n");
        return false;
    }
}

void decode_huffman_data(Image* image, Bit_Reader* reader) {
    printf("Decoding Huffman data...\n");
    
    // Generate Huffman codes for all tables
    for (uint i = 0; i < 4; ++i) {
        if (image->huffman_dc_tables[i].is_set) {
            generate_huffman_codes(&image->huffman_dc_tables[i]);
        }
        if (image->huffman_ac_tables[i].is_set) {
            generate_huffman_codes(&image->huffman_ac_tables[i]);
        }
    }
    
    // Previous DC values for each component (Y, Cb, Cr)
    int previous_dcs[3] = {0, 0, 0};
    uint skips = 0;
    
    // For baseline JPEG, all components are in the scan
    // For progressive, only some components may be present
    const bool luminance_only = image->components_in_scan == 1 && 
                                image->color_components[0].used_in_scan;
    
    const uint y_step = luminance_only ? 1 : image->vertical_sampling_factor;
    const uint x_step = luminance_only ? 1 : image->horizontal_sampling_factor;
    const uint restart_interval = image->restart_interval * x_step * y_step;
    
    // Decode all MCUs (Minimum Coded Units)
    for (uint y = 0; y < image->block_height; y += y_step) {
        for (uint x = 0; x < image->block_width; x += x_step) {
            // Handle restart intervals
            if (restart_interval != 0 && 
                (y * image->block_width_real + x) % restart_interval == 0) {
                previous_dcs[0] = 0;
                previous_dcs[1] = 0;
                previous_dcs[2] = 0;
                skips = 0;
                align_reader(reader);
            }
            
            // Decode each component in the scan
            for (uint i = 0; i < image->num_components; ++i) {
                const Color_Component* component = &image->color_components[i];
                
                if (component->used_in_scan) {
                    const uint v_max = luminance_only ? 1 : component->ver_sampling_factor;
                    const uint h_max = luminance_only ? 1 : component->hor_sampling_factor;
                    
                    for (uint v = 0; v < v_max; ++v) {
                        for (uint h = 0; h < h_max; ++h) {
                            uint block_index = (y + v) * image->block_width_real + (x + h);
                            
                            int* block_component = NULL;
                            switch(i) {
                                case 0: block_component = image->blocks[block_index].y; break;
                                case 1: block_component = image->blocks[block_index].cb; break;
                                case 2: block_component = image->blocks[block_index].cr; break;
                            }
                            
                            if (!decode_block_component(
                                    image,
                                    reader,
                                    block_component,
                                    &previous_dcs[i],
                                    &skips,
                                    &image->huffman_dc_tables[component->huffman_dc_table_id],
                                    &image->huffman_ac_tables[component->huffman_ac_table_id])) {
                                fprintf(stderr, "Error - Failed to decode block at (%u, %u)\n", x, y);
                                image->valid = false;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    
    printf("Huffman decoding complete!\n");
}