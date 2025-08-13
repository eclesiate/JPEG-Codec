#include "../inc/decoder.h"
#include <string.h>

Header* read_JPEG(const char* filename) {
    if (!access(filename, F_OK)) {
        FILE* jpeg = fopen(filename, "rb");
        if (jpeg == NULL) {
            fprintf(stderr, "Error: failed to open %s", filename);
            exit(1);
        }

        Header* header = calloc(1, sizeof(Header)); // * WHEN MALLOC-ING, MAKE SURE TO MALLOC THE SIZE OF THE STRUCT, NOT THE POINTER
        init_header(header);
        

        byte marker_FF = (byte) fgetc(jpeg);
        byte SOI_marker = (byte) fgetc(jpeg);
        if (marker_FF != 0xFF && SOI_marker != SOI) {
            fprintf(stderr, "Error: %s not a valid jpeg\n", filename);
            fclose(jpeg);
            exit(1);
        }

        byte marker1 = (byte) fgetc(jpeg);
        byte marker2 = (byte) fgetc(jpeg);

        read_next_marker(jpeg, &marker1, &marker2);

        while(marker2 != EOI) { // loop till end of image
            
            if (ferror(jpeg)) {
                fprintf(stderr, "Error: no EOF marker, or read error in %s\n", filename);
                exit(1);
            }
            // printf("marker : 0x%02x%02x\n", marker1, marker2);
            if (marker1 != 0XFF) {
                fprintf(stderr, "Error: unable to read marker without FF in %s\n", filename);
                exit(1);
            } 
            else if (marker2 == SOF0) {
                header->frame_type = SOF0;
                read_sof_marker(header, jpeg);
                
            }
            else if (marker2 == DQT) {
                read_quantization_table(header, jpeg);
            }
            else if (marker2 == DHT) {
                // Skip over a Define-Huffman-Table segment
                uint len = (fgetc(jpeg) << 8) | fgetc(jpeg);
                fseek(jpeg, len - 2, SEEK_CUR);
            }
            else if (marker2 == DRI) {
                read_restart_interval(header, jpeg);
            }

            // * APPN segment aren't needed for decoding
            else if(marker2 >= APP0 && marker2 <= APP15) {
                skip_APPN(jpeg);
            } 

            marker1 = (byte) fgetc(jpeg);
            marker2 = (byte) fgetc(jpeg);
            read_next_marker(jpeg, &marker1, &marker2);

        }
        return header;
    
    } else {
        fprintf(stderr, "Error: invalid file path: %s", filename);
        exit(1);
    }
}

void read_restart_interval(Header* const header, FILE* jpeg) {
    printf("Reading DRI marker\n");
    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg);
    header->restart_interval = (fgetc(jpeg) << 8) | fgetc(jpeg);

    if (length != 4) {
        fprintf(stderr, "Error: invalid dri length\n");
        exit(1);
    }
}

void read_next_marker(FILE* jpeg, byte* marker1, byte* marker2) {
    // Read until we hit 0xFF
    do {
        *marker1 = fgetc(jpeg);
    } while (*marker1 != 0xFF && !feof(jpeg));

    // Skip any padding FFs (some JPEGs have multiple FFs before marker code)
    do {
        *marker2 = fgetc(jpeg);
    } while (*marker2 == 0xFF && !feof(jpeg));

    if (feof(jpeg)) {
        *marker1 = 0;
        *marker2 = 0;
    }
}

void skip_APPN(FILE* jpeg) {
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
    
        byte table_precision = (table_info >> 4) & 0x0F; //8b or 16b table
        byte table_id = table_info & 0x0F;
        if (table_id > 3) {
            fprintf(stderr, "Error: invalid quantization table id: %ud", (uint) table_precision);
            exit(1);
        }
        
        header->quantization_tables[table_id].valid = true;

        if (table_precision != 0) { // 16b precision
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                header->quantization_tables[table_id].table[zig_zag_map[i]] = (fgetc(jpeg) << 8) | fgetc(jpeg);
            }
            length -= 128; // 16b precision is 2 bytes per table element
        } 
        else {
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                header->quantization_tables[table_id].table[zig_zag_map[i]] = fgetc(jpeg);
            }
            length -= 64;
        }
    }

    if (length != 0) {
        fprintf(stderr, "Error: invalid DQT marker\n");
        exit(1);
    }
}

void print_header(const Header* const header) {
    if (header == NULL) return;

    printf("DQT-----------------------\n");
    for (size_t i = 0; i < MAX_QUANTIZATION_TABLES; i++) {
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

    printf("DRI------------\n");
    printf("Restart Interval: %ud\n", header->restart_interval);
    
    printf("SOF------------\n");
    printf("Frame Type: 0x%08x\n", header->frame_type);
    printf("Height: %d\n", header->height);
    printf("Width: %d\n", header->width);
    printf("Color Components: \n");
    for (uint i = 0; i < header->num_components; ++i) {
        if (header->color_components[i].used) {
            printf("Component ID: %d\n", (i + 1));
            printf("Horizontal Sampling Factor: %d\n", (uint)header->color_components[i].hor_sampling_factor);
            printf("Vertical Sampling Factor: %d\n", (uint)header->color_components[i].ver_sampling_factor);
            printf("Quantization Table ID: %d\n", (uint)header->color_components[i].quantization_table_id);
        }
    }
}

void read_sof_marker(Header* const header, FILE* jpeg) {
    printf("Starting to read SOF marker\n");
    if (header->num_components != 0) {
        fprintf(stderr, "Error: multiple SOFs read\n");
        exit(1);
    }

    uint length = (fgetc(jpeg) << 8) | (fgetc(jpeg));

    byte precision = fgetc(jpeg);
    if (precision != 8) { // for now, ill only support baseline JPEGs
        fprintf(stderr, "Error: invalid frame precision\n");
        exit(1);    
    }

    header->height = (fgetc(jpeg) << 8) | fgetc(jpeg);
    header->width = (fgetc(jpeg) << 8) | fgetc(jpeg);

    header->num_components = fgetc(jpeg);
    if (header->num_components == 0){
        fprintf(stderr, "Error: invalid number of color components\n");
        exit(1);
    }

    for (uint i = 0; i < header->num_components; i++) {
        byte component_id = fgetc(jpeg);
        // allow some flexibility, since some component ids are zero based
        if(component_id == 0) {
            header->zero_based = true;
        }

        if (header->zero_based) {
            ++component_id;
        }
        if (component_id == 0 || component_id > 3) {
            fprintf(stderr, "Error: invalid color component ids\n");
            exit(1);    
        }
        Color_Component* component = &header->color_components[component_id - 1]; // since component_id is between 1,2, and 3
        if (component->used) {
            fprintf(stderr, "Error: Duplicate color component_id\n");
            exit(1);
        }
        component->used = true;
        byte sampling_factor = fgetc(jpeg);
        component->hor_sampling_factor = sampling_factor >> 4;
        component->ver_sampling_factor = sampling_factor & 0x0F;
        if (component->hor_sampling_factor != 1 || component->ver_sampling_factor != 1) {
            fprintf(stderr, "Error: invalid horizontal or vertical sampling factors\n");
            exit(1);
        }

        component->quantization_table_id = fgetc(jpeg);
        
        if(component->quantization_table_id > 3) {
            fprintf(stderr, "Error: invalid quantization table id in frome component\n");
            exit(1);
        } 
    }
    if (length - 8 - (3*header->num_components) != 0) {
        fprintf(stderr, "Error: invalid SOF\n");
        exit(1);
    } 

}
 
void init_header(Header* const header) {
    header->valid = true;
    header->frame_type = 0;
    header->height = 0;
    header->width = 0;
    header->num_components = 0;
    header->zero_based = false;
    header->restart_interval = 0;
    memset(header->color_components, 0, sizeof(header->color_components));
    memset(header->quantization_tables, 0, sizeof(header->quantization_tables));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: not enough arguments\n");
        exit(1);
    } else {
        for (int i = 1; i < argc; ++i) {
            const char* filename = argv[i];
            Header* header = read_JPEG(filename);
            print_header(header);
            free(header);
        }
    }
}