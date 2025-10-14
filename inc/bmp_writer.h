#ifndef BMP_WRITER_H
#define BMP_WRITER_H

#include "jpeg.h"

// Write the decoded image data to a BMP file
// This writes the raw DCT coefficients as grayscale for debugging
// (before dequantization, IDCT, and color conversion)
void write_bmp(const Image* image, const char* filename);

// Write only the DC coefficients as a thumbnail
void write_bmp_dc_only(const Image* image, const char* filename);

#endif