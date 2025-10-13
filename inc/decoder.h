#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"

image* read_JPEG(const char* filename);

void skip_APPN(FILE* jpeg);

void read_quantization_table(image* image, FILE* jpeg);

void print_image(const image* const image);

void read_sof_marker(image* const image, FILE* jpeg);

void read_next_marker(FILE* jpeg, byte* marker1, byte* marker2);

void read_restart_interval(image* image, FILE* jpeg);

void read_huffman_table(image* image, FILE* jpeg);

void read_sos_marker(image* image, FILE* jpeg);

void print_scan_info(const image* const image);

void init_image(image* image);

void skip_comment(FILE* jpeg);

void skip_unused_marker(FILE* jpeg);