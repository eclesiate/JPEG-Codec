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

        while(marker2 != EOI) {
            if (ferror(jpeg)) {
                fprintf(stderr, "Error: no EOF marker, or read error in %s\n", filename);
                exit(1);
            }
            if (marker1 != 0XFF) {
                fprintf(stderr, "Error: unable to read marker without FF in %s\n", filename);
                exit(1);
            } 
            else if(marker2 >= APP0 && marker2 <= APP15) {
                read_APPN(header, jpeg);
                printf("finished reading app0");
                break;
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

void read_APPN(Header* header, FILE* jpeg) {

    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg); // * length is in big endian 
    for (uint i = 0; i < length; ++i) {
        fgetc(jpeg); // advance file stream ptr
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
        }
        
    }
}