#include <stdio.h> 
#include <fcntl.h>
#include <unistd.h>

#include "jpeg.h"

Header* read_JPEG(const char* filename);

void read_APPN(Header* const header, FILE* jpeg);