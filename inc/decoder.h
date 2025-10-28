#ifndef DECODER_H
#define DECODER_H

#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"
#include "bit_reader.h"
#include "huffman_decoder.h"
#include "bmp_writer.h"

void init_image(Image* image);

// return image with decoded data
Image* read_JPEG(const char* filename);

void skip_unused_markers(FILE* jpeg, byte marker);

void read_quantization_table(Image* image, FILE* jpeg);

void read_huffman_table(Image* image, FILE* jpeg);

void read_sof_marker(Image* const image, FILE* jpeg);

void read_restart_interval(Image* image, FILE* jpeg);

void read_sos_marker(Image* image, FILE* jpeg);

void print_image(const Image* const image);

// for progressive jpegs.

void read_scans(Image* image, Bit_Reader* reader);

void print_scan_info(const Image* const image);

#endif