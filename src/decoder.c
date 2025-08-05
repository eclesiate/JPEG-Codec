#include "../inc/decoder.h"

Header* read_JPEG(const char* filename) {
    if (!access(filename, F_OK)) {
        FILE* jpeg = fopen(filename, "rb");
        if (jpeg == NULL) {
            fprintf(stderr, "Error: failed to open %s", filename);
            exit(1);
        }

        Header* header = malloc(sizeof(header));
        header->valid = true;

        byte marker_FF = (byte) fgetc(jpeg);
        byte SOI_marker = (byte) fgetc(jpeg);
        if (marker_FF != 0xFF && SOI_marker != SOI) {
            fprintf(stderr, "Error: %s not a valid jpeg\n", filename);
            fclose(jpeg);
            exit(1);
        }

        byte marker1 = (byte) fgetc(jpeg);
        byte marker2 = (byte) fgetc(jpeg);

        while(marker2 != EOI) { // loop till end of image
            if (ferror(jpeg)) {
                fprintf(stderr, "Error: no EOF marker, or read error in %s\n", filename);
                exit(1);
            }
            if (marker1 != 0XFF) {
                fprintf(stderr, "Error: unable to read marker without FF in %s\n", filename);
                exit(1);
            } 
            else if (marker2 == DQT) {
                read_quantization_table(header, jpeg);
                break;
            }
            // * APPN segment aren't needed for decoding
            else if(marker2 >= APP0 && marker2 <= APP15) {
                skip_APPN(header, jpeg);
                printf("finished reading appN\n");
            } 

            marker1 = (byte) fgetc(jpeg);
            marker2 = (byte) fgetc(jpeg);
        }
        return header;
    
    } else {
        fprintf(stderr, "Error: invalid file path: %s", filename);
        exit(1);
    }
}

void skip_APPN(Header* header, FILE* jpeg) {

    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg); // * length is in big endian 
    for (uint i = 0; i < length - 2; ++i) {
        fgetc(jpeg); // advance file stream ptr
    }
}

void read_quantization_table(Header* header, FILE* jpeg) {
    int length = (fgetc(jpeg) << 8) | fgetc(jpeg);
    length -= 2; // TODO signed or unsigned len?

    while (length > 0) {
        byte table_info = fgetc(jpeg);
        --length;
        byte table_precision = table_info & 0x0F; //8b or 16b table
        if (table_precision > 3) {
            fprintf(stderr, "Error: invalid quantization table precision: %ud", (uint) table_precision);
            exit(1);
        }

        header->quantization_tables[table_precision].valid = true;

        if (table_info >> 4 != 0) {
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                header->quantization_tables[table_precision].table[zig_zag_map[i]] = (fgetc(jpeg) << 8) | fgetc(jpeg);
            }
            length -= 128;
        } 
        else {
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                header->quantization_tables[table_precision].table[zig_zag_map[i]] = fgetc(jpeg);
            }
            length -= 64;
        }
        
    }

    if (length != 0) {
        fprintf(stderr, "Error: invalid DQT marker\n");
        exit(1);
    }
}

void print_header(const Header* const header ) {
    if (header == NULL) return;

    printf("DQT-----------------------\n");

    for (size_t i = 0; i < 4; i++) {
        if (header->quantization_tables[i].valid) {
            printf("Table ID: %ld\n", i);
            printf("Table Data: ");
            for (size_t j = 0; j < 64; ++j) {
                if (j%8 == 0) {
                    printf("\n");
                }
                printf("%d ", header->quantization_tables[i].table[j]);
            }
            printf("\n");
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: not enough arguments\n");
        exit(1);
    } else {
        for (size_t i = 1; i < argc; ++i) {
            const char* filename = argv[i];
            Header* header = read_JPEG(filename);
            printf("is jpeg\n");
            print_header(header);

        }
        
    }
}