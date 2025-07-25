#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"

Header* read_JPEG(const char* filename);

void skip_APPN(Header* const header, FILE* jpeg);

void read_quantization_table(Header* header, FILE* jpeg);