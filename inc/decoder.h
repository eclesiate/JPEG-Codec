#ifndef DECODER_H
#define DECODER_H

#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"
#include "bit_reader.h"
#include "huffman_decoder.h"
#include "bmp_writer.h"

// Read JPEG file and return image with decoded data
Image* read_JPEG(const char* filename);

// Skip APPN markers
void skip_APPN(FILE* jpeg);

// Read quantization table
void read_quantization_table(Image* image, FILE* jpeg);

// Print frame information
void print_image(const Image* const image);

// Read Start of Frame marker
void read_sof_marker(Image* const image, FILE* jpeg);

// Read next marker from file
void read_next_marker(FILE* jpeg, byte* marker1, byte* marker2);

// Read restart interval
void read_restart_interval(Image* image, FILE* jpeg);

// Read Huffman table
void read_huffman_table(Image* image, FILE* jpeg);

// Read Start of Scan marker
void read_sos_marker(Image* image, FILE* jpeg);

// Print scan information
void print_scan_info(const Image* const image);

// Initialize image structure
void init_image(Image* image);

// Skip comment marker
void skip_comment(FILE* jpeg);

// Skip unused markers
void skip_unused_markers(FILE* jpeg);

// Read all scans (including Huffman data)
void read_scans(Image* image, Bit_Reader* reader);

#endif