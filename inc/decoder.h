#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"

Header* read_JPEG(const char* filename);

void skip_APPN(FILE* jpeg);

void read_quantization_table(Header* header, FILE* jpeg);

void print_header(const Header* const header);

void read_sof_marker(Header* const header, FILE* jpeg);

void read_next_marker(FILE* jpeg, byte* marker1, byte* marker2);

void  read_restart_interval(Header* header, FILE* jpeg);

void init_header(Header* header);
