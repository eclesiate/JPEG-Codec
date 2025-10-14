#include "../inc/decoder.h"
#include <string.h>

Image* read_JPEG(const char* filename) {
    if (!access(filename, F_OK)) {
        FILE* jpeg = fopen(filename, "rb");
        if (jpeg == NULL) {
            fprintf(stderr, "Error: failed to open %s", filename);
            exit(1);
        }

        Image* image = calloc(1, sizeof(Image)); // * WHEN MALLOC-ING, MAKE SURE TO MALLOC THE SIZE OF THE STRUCT, NOT THE POINTER
        init_image(image);
        
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

        while(image->valid) { // loop till end of image
            
            if (ferror(jpeg)) {
                fprintf(stderr, "Error: no EOF marker, or read error in %s\n", filename);
                exit(1);
            }
            // printf("marker : 0x%02x%02x\n", marker1, marker2);
            if (marker1 != 0XFF) {
                fprintf(stderr, "Error: unable to read marker without FF in %s\n", filename);
                exit(1);
            } 
            else if (marker2 == SOF0) { // baseline jpeg
                image->frame_type = SOF0;
                read_sof_marker(image, jpeg);
            }
            else if (marker2 == SOF2) { // progressive jpeg
                image->frame_type = SOF2;
                read_sof_marker(image, jpeg);
            }
            else if (marker2 == DQT) {
                read_quantization_table(image, jpeg);
            }
            else if (marker2 == DHT) {
                read_huffman_table(image, jpeg);
            }
            else if (marker2 == DRI) { // define restart interval, if theres like corruption in data stream this can be used to resync reading
                read_restart_interval(image, jpeg);
            }
            else if (marker2 == SOS) {
                break;
            }
            // * APPN segment aren't needed for decoding
            else if(marker2 >= APP0 && marker2 <= APP15) {
                skip_unused_markers(jpeg);
            } 
            else if (marker2 == COM) {
                skip_unused_markers(jpeg);
            }
            else if ((marker2 >= JPG0 && marker2 <= JPG13) ||
                    marker2 == DNL || marker2 == DHP || marker2 == EXP) {
                skip_unused_markers(jpeg);
            }
            // ignore multiple 0xFFs in a row
            else if (marker2 == 0xFF) {
                marker2 = fgetc(jpeg);
                continue;
            }
            else if (marker2 == EOI) {
                fprintf(stderr, "Error: EOI detected before SOS\n");
                image->valid = false;
                fclose(jpeg);
                return image;
            }
            else if (marker2 == DAC) {
                // TODO define arithmetic coding
                image->valid = false;
                fclose(jpeg);
                return image;
            }
            else if (marker2 >= SOF0 && marker2 <= SOF15) {
                fprintf(stderr, "Error: SOF marker not supported: 0x%02x\n", marker2);
                image->valid = false;
                fclose(jpeg);
                return image;
            }
            else if (marker2 >= RST0 && marker2 <= RST7) {
                fprintf(stderr, "Error: restart marker detected before SOS\n");
                image->valid = false;
                fclose(jpeg);
                return image;
            }
            else {
                fprintf(stderr, "Error: unknown marker: 0x%02x\n", marker2);
                image->valid = false;
                fclose(jpeg);
                return image;
            }


            marker1 = (byte) fgetc(jpeg);
            marker2 = (byte) fgetc(jpeg);
            read_next_marker(jpeg, &marker1, &marker2);
        }

        if (image->valid && marker2 == SOS) {
            read_sos_marker(image, jpeg);
        }
        // Allocate block array
        image->blocks = calloc(image->block_height_real * image->block_width_real, sizeof(Block));
        if (image->blocks == NULL) {
            fprintf(stderr, "Error - Memory allocation failed for blocks\n");
            image->valid = false;
            fclose(jpeg);
            return image;
        }
        
        // Create bit reader for Huffman decoding
        Bit_Reader reader;
        init_bit_reader(&reader, jpeg);
        
        // Read and decode scans
        read_scans(image, &reader);
        
        fclose(jpeg);
        return image;
    
    } else {
        fprintf(stderr, "Error: invalid file path: %s", filename);
        exit(1);
    }
}

void read_scans(Image* image, Bit_Reader* reader) {
    // Read first SOS marker and decode
    read_sos_marker(image, reader->file);
    if (!image->valid) {
        return;
    }
    
    print_scan_info(image);
    decode_huffman_data(image, reader);
    
    if (!image->valid) {
        return;
    }
    
    // For progressive JPEGs, there may be additional scans
    // For now, we only support baseline (single scan)
    if (image->frame_type == SOF2) {
        fprintf(stderr, "Warning: Progressive JPEG - only first scan decoded\n");
    }
    
    // Read until EOI
    byte marker1 = fgetc(reader->file);
    byte marker2 = fgetc(reader->file);
    read_next_marker(reader->file, &marker1, &marker2);
    
    while (image->valid) {
        if (feof(reader->file)) {
            fprintf(stderr, "Error: file ended prematurely\n");
            image->valid = false;
            return;
        }
        
        if (marker1 != 0xFF) {
            fprintf(stderr, "Error: expected a marker\n");
            image->valid = false;
            return;
        }
        
        if (marker2 == EOI) {
            printf("Found EOI marker - decoding complete\n");
            break;
        }
        // Progressive JPEGs may have additional DHT, SOS, DRI markers
        else if (marker2 == DHT && image->frame_type == SOF2) {
            read_huffman_table(image, reader->file);
        }
        else if (marker2 == SOS && image->frame_type == SOF2) {
            // Additional scan - not implemented yet
            fprintf(stderr, "Warning: Additional scan detected but not decoded\n");
            break;
        }
        else if (marker2 == DRI && image->frame_type == SOF2) {
            read_restart_interval(image, reader->file);
        }
        else if (marker2 >= RST0 && marker2 <= RST7) {
            // Restart marker at end of scan - ignore
        }
        else if (marker2 == 0xFF) {
            marker2 = fgetc(reader->file);
            continue;
        }
        else {
            fprintf(stderr, "Error: invalid marker after scan: 0x%02x\n", marker2);
            image->valid = false;
            return;
        }
        
        marker1 = fgetc(reader->file);
        marker2 = fgetc(reader->file);
        read_next_marker(reader->file, &marker1, &marker2);
    }
}

void read_restart_interval(Image* const image, FILE* jpeg) {
    printf("Reading DRI marker\n");
    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg);
    image->restart_interval = (fgetc(jpeg) << 8) | fgetc(jpeg);

    if (length != 4) {
        fprintf(stderr, "Error: invalid dri length\n");
        return;
    }
}

void read_next_marker(FILE* jpeg, byte* marker1, byte* marker2) {
    *marker1 = fgetc(jpeg);
    while (*marker1 != 0xFF && !feof(jpeg)) {
        *marker1 = fgetc(jpeg);
    }

    // Skip any padding FFs (some JPEGs have multiple FFs before marker code)
    do {
        *marker2 = fgetc(jpeg);
    } while (*marker2 == 0xFF && !feof(jpeg));

    if (feof(jpeg)) {
        *marker1 = 0;
        *marker2 = 0;
    }
}

void skip_unused_markers(FILE* jpeg) {
    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg); // * length is in big endian 
    if (length < 2) {
        fprintf(stderr, "Error: invalid comment length\n");
        exit(1);
    }
    for (uint i = 0; i < length - 2; ++i) {
        fgetc(jpeg); // advance file stream ptr
    }
}

void read_quantization_table(Image* image, FILE* jpeg) {
    int length = (fgetc(jpeg) << 8) | fgetc(jpeg);
    
    length -= 2; // TODO signed or unsigned len?

    while (length > 0) {
        byte table_info = fgetc(jpeg);
        --length;
    
        byte table_precision = (table_info >> 4) & 0x0F; //8b or 16b table
        byte table_id = table_info & 0x0F;
        if (table_id > 3) {
            fprintf(stderr, "Error: invalid quantization table id: %ud", (uint) table_precision);
            return;
        }
        
        image->quantization_tables[table_id].valid = true;

        if (table_precision != 0) { // 16b precision
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                image->quantization_tables[table_id].table[zig_zag_map[i]] = (fgetc(jpeg) << 8) | fgetc(jpeg);
            }
            length -= 128; // 16b precision is 2 bytes per table element
        } 
        else { // 8b precision
            for (uint i = 0; i < QUANTIZATION_TABLE_SZ; ++i) {
                image->quantization_tables[table_id].table[zig_zag_map[i]] = fgetc(jpeg);
            }
            length -= 64;
        }
    }

    if (length != 0) {
        fprintf(stderr, "Error: invalid DQT marker\n");
        return;
    }
}

void print_image(const Image* const image) {
    if (image == NULL) return;

    printf("SOF------------\n");
    printf("Frame Type: 0x%02x\n", image->frame_type);
    printf("Height: %d\n", image->height);
    printf("Width: %d\n", image->width);
    printf("Color Components:\n");
    for (uint i = 0; i < image->num_components; ++i) {
        if (image->color_components[i].used_in_frame) {
            printf("Component ID: %d\n", (i + 1));
            printf("Horizontal Sampling Factor: %d\n", 
                   (uint)image->color_components[i].hor_sampling_factor);
            printf("Vertical Sampling Factor: %d\n", 
                   (uint)image->color_components[i].ver_sampling_factor);
            printf("Quantization Table ID: %d\n", 
                   (uint)image->color_components[i].quantization_table_id);
        }
    }

    printf("DQT------------\n");
    for (size_t i = 0; i < MAX_QUANTIZATION_TABLES; i++) {
        if (image->quantization_tables[i].valid) {
            printf("Table ID: %ld\n", i);
            printf("Table Data:");
            for (size_t j = 0; j < 64; ++j) {
                if (j % 8 == 0) {
                    printf("\n");
                }
                printf("%d ", image->quantization_tables[i].table[j]);
            }
            printf("\n");
        }
    }

    printf("DHT------------\n");
    printf("DC Tables:\n");
    for (uint i = 0; i < 4; ++i) {
        if (image->huffman_dc_table[i].is_set) {
            printf("Table ID: %d\n", i);
            printf("Symbols:\n");
            for (uint j = 0; j < 16; ++j) {
                printf("%d: ", (j + 1));
                for (uint k = image->huffman_dc_table[i].offsets[j]; 
                     k < image->huffman_dc_table[i].offsets[j + 1]; ++k) {
                    printf("0x%02x ", (uint)image->huffman_dc_table[i].symbols[k]);
                }
                printf("\n");
            }
        }
    }
    printf("AC Tables:\n");
    for (uint i = 0; i < 4; ++i) {
        if (image->huffman_ac_tables[i].is_set) {
            printf("Table ID: %d\n", i);
            printf("Symbols:\n");
            for (uint j = 0; j < 16; ++j) {
                printf("%d: ", (j + 1));
                for (uint k = image->huffman_ac_tables[i].offsets[j]; 
                     k < image->huffman_ac_tables[i].offsets[j + 1]; ++k) {
                    printf("0x%02x ", (uint)image->huffman_ac_tables[i].symbols[k]);
                }
                printf("\n");
            }
        }
    }

    printf("DRI------------\n");
    printf("Restart Interval: %d\n", image->restart_interval);
}

void print_scan_info(const Image* const image) {
    if (image == NULL) return;
    
    printf("SOS------------\n");
    printf("Start of Selection: %d\n", image->start_of_selection);
    printf("End of Selection: %d\n", image->end_of_selection);
    printf("Successive Approximation High: %d\n", image->successive_approx_high);
    printf("Successive Approximation Low: %d\n", image->successive_approx_low);
    printf("Components in Scan: %d\n", image->components_in_scan);
    printf("Color Components:\n");
    for (uint i = 0; i < image->num_components; ++i) {
        if (image->color_components[i].used_in_scan) {
            printf("Component ID: %d\n", (i + 1));
            printf("Huffman DC Table ID: %d\n", image->color_components[i].huffman_dc_table_id);
            printf("Huffman AC Table ID: %d\n", image->color_components[i].huffman_ac_table_id);
        }
    }
}

void read_sos_marker(Image* const image, FILE* jpeg) {
    printf("Reading SOS marker\n");
    
    if (image->num_components == 0) {
        fprintf(stderr, "Error: SOS detected before SOF\n");
        image->valid = false;
        return;
    }

    uint length = (fgetc(jpeg) << 8) | fgetc(jpeg);

    // Reset all components scan usage flags for progressive jpeg
    for (uint i = 0; i < image->num_components; ++i) {
        image->color_components[i].used_in_scan = false;
    }

    // Number of components in this scan
    image->components_in_scan = fgetc(jpeg);
    if (image->components_in_scan == 0) {
        fprintf(stderr, "Error: scan must include at least 1 component\n");
        image->valid = false;
        return;
    }

    // Read component-specific scan data
    for (uint i = 0; i < image->components_in_scan; ++i) {
        byte component_id = fgetc(jpeg);
        
        // Handle zero-based component IDs
        if (image->zero_based) {
            component_id += 1;
        }
        
        if (component_id == 0 || component_id > image->num_components) {
            fprintf(stderr, "Error: invalid color component ID in SOS: %d\n", component_id);
            image->valid = false;
            return;
        }

        Color_Component* component = &image->color_components[component_id - 1];
        
        if (!component->used_in_frame) {
            fprintf(stderr, "Error: component %d not defined in frame\n", component_id);
            image->valid = false;
            return;
        }
        
        if (component->used_in_scan) {
            fprintf(stderr, "Error: duplicate component ID in SOS: %d\n", component_id);
            image->valid = false;
            return;
        }
        component->used_in_scan = true;

        byte huffman_table_ids = fgetc(jpeg);
        component->huffman_dc_table_id = huffman_table_ids >> 4;
        component->huffman_ac_table_id = huffman_table_ids & 0x0F;

        if (component->huffman_dc_table_id > 3) {
            fprintf(stderr, "Error: invalid Huffman DC table ID: %d\n", component->huffman_dc_table_id);
            image->valid = false;
            return;
        }
        if (component->huffman_ac_table_id > 3) {
            fprintf(stderr, "Error: invalid Huffman AC table ID: %d\n", component->huffman_ac_table_id);
            image->valid = false;
            return;
        }
    }

    // Read spectral selection and successive approximation
    image->start_of_selection = fgetc(jpeg);
    image->end_of_selection = fgetc(jpeg);
    byte successive_approx = fgetc(jpeg);
    image->successive_approx_high = successive_approx >> 4;
    image->successive_approx_low = successive_approx & 0x0F;

    // Validate based on frame type
    if (image->frame_type == SOF0) {
        // Baseline JPEG doesn't use spectral selection or successive approximation
        if (image->start_of_selection != 0 || image->end_of_selection != 63) {
            fprintf(stderr, "Error: invalid spectral selection for baseline JPEG\n");
            image->valid = false;
            return;
        }
        if (image->successive_approx_high != 0 || image->successive_approx_low != 0) {
            fprintf(stderr, "Error: invalid successive approximation for baseline JPEG\n");
            image->valid = false;
            return;
        }
    }
    else if (image->frame_type == SOF2) {
        // Progressive JPEG validation
        if (image->start_of_selection > image->end_of_selection) {
            fprintf(stderr, "Error: start of selection > end of selection\n");
            image->valid = false;
            return;
        }
        if (image->end_of_selection > 63) {
            fprintf(stderr, "Error: end of selection > 63\n");
            image->valid = false;
            return;
        }
        if (image->start_of_selection == 0 && image->end_of_selection != 0) {
            fprintf(stderr, "Error: spectral selection contains both DC and AC\n");
            image->valid = false;
            return;
        }
        if (image->start_of_selection != 0 && image->components_in_scan != 1) {
            fprintf(stderr, "Error: AC scan must contain only 1 component\n");
            image->valid = false;
            return;
        }
        if (image->successive_approx_high != 0 &&
            image->successive_approx_low != image->successive_approx_high - 1) {
            fprintf(stderr, "Error: invalid successive approximation\n");
            image->valid = false;
            return;
        }
    }

    // Validate that all components in scan have necessary tables
    for (uint i = 0; i < image->num_components; ++i) {
        const Color_Component* component = &image->color_components[i];
        if (component->used_in_scan) {
            // Check quantization table
            if (!image->quantization_tables[component->quantization_table_id].valid) {
                fprintf(stderr, "Error: component using uninitialized quantization table\n");
                image->valid = false;
                return;
            }
            // Check DC table if needed
            if (image->start_of_selection == 0) {
                if (!image->huffman_dc_table[component->huffman_dc_table_id].is_set) {
                    fprintf(stderr, "Error: component using uninitialized Huffman DC table\n");
                    image->valid = false;
                    return;
                }
            }
            // Check AC table if needed
            if (image->end_of_selection > 0) {
                if (!image->huffman_ac_tables[component->huffman_ac_table_id].is_set) {
                    fprintf(stderr, "Error: component using uninitialized Huffman AC table\n");
                    image->valid = false;
                    return;
                }
            }
        }
    }

    // Validate length
    if (length - 6 - (2 * image->components_in_scan) != 0) {
        fprintf(stderr, "Error: invalid SOS length\n");
        image->valid = false;
        return;
    }
}

void read_sof_marker(Image* const image, FILE* jpeg) {
    printf("Starting to read SOF marker\n");
    if (image->num_components != 0) {
        fprintf(stderr, "Error: multiple SOFs read\n");
        return;
    }

    uint length = (fgetc(jpeg) << 8) | (fgetc(jpeg));

    byte precision = fgetc(jpeg);
    if (precision != 8) { // for now, ill only support baseline JPEGs
        fprintf(stderr, "Error: invalid frame precision\n");
        return;    
    }

    image->height = (fgetc(jpeg) << 8) | fgetc(jpeg);
    image->width = (fgetc(jpeg) << 8) | fgetc(jpeg);

    image->num_components = fgetc(jpeg);
    if (image->num_components == 0){
        fprintf(stderr, "Error: invalid number of color components\n");
        return;
    }

    for (uint i = 0; i < image->num_components; i++) {
        byte component_id = fgetc(jpeg);
        // allow some flexibility, since some component ids are zero based
        if(component_id == 0) {
            image->zero_based = true;
        }

        if (image->zero_based) {
            ++component_id;
        }
        if (component_id == 0 || component_id > 3) {
            fprintf(stderr, "Error: invalid color component ids\n");
            return;    
        }
        Color_Component* component = &image->color_components[component_id - 1]; // since component_id is between 1,2, and 3
        if (component->used_in_frame) {
            fprintf(stderr, "Error: Duplicate color component_id\n");
            return;
        }
        component->used_in_frame = true;
        byte sampling_factor = fgetc(jpeg);
        component->hor_sampling_factor = sampling_factor >> 4;
        component->ver_sampling_factor = sampling_factor & 0x0F;
        if (component->hor_sampling_factor != 1 || component->ver_sampling_factor != 1) {
            fprintf(stderr, "Error: invalid horizontal or vertical sampling factors\n");
            return;
        }

        component->quantization_table_id = fgetc(jpeg);
        
        if(component->quantization_table_id > 3) {
            fprintf(stderr, "Error: invalid quantization table id in frome component\n");
            return;
        } 
    }
    if (length - 8 - (3*image->num_components) != 0) {
        fprintf(stderr, "Error: invalid SOF\n");
        return;
    } 

}

void read_huffman_table(Image* const image, FILE* jpeg) {
    printf("Reading DHT marker\n");
    int length = (fgetc(jpeg) << 8) | (fgetc(jpeg));
    length -= 2; // total segment length is including the 2B for itself

    while (length > 0) { 
        byte table_info = fgetc(jpeg);
        byte table_id = table_info & 0x0F;
        bool is_ac_table = table_info >> 4;

        if (table_id > 3) {
            fprintf(stderr, "Error: invalid huffman table id\n");
            image->valid = false;
            return;
        }

        Huffman_Table* ht;
        if (is_ac_table) {
            ht = &image->huffman_ac_tables[table_id];
        }
        else {
            ht = &image->huffman_dc_table[table_id];
        }
        ht->is_set = true;

        ht->offsets[0] = 0;
        uint all_symbols = 0;
        for (uint i = 1; i <= 16; i++) {
            all_symbols += fgetc(jpeg);
            ht->offsets[i] = all_symbols;
        }

        if (all_symbols > 162) {
            fprintf(stderr, "Error: too many symbols in huff table\n");
            image->valid = false; 
            return;
        }

        for (uint i = 0; i < all_symbols; ++i) {
            ht->symbols[i] = fgetc(jpeg);
        }

        length -= 17 + all_symbols;

    } 
    if (length) {
        fprintf(stderr, "Error: invalid DHT\n");
        image->valid = false;
    }
}
 
void init_image(Image* const image) {
    image->valid = true;
    image->frame_type = 0;
    image->height = 0;
    image->width = 0;
    image->num_components = 0;
    image->zero_based = false;
    image->start_of_selection = 0;
    image->end_of_selection = 0;
    image->successive_approx_high = 0;
    image->successive_approx_low = 0;
    image->restart_interval = 0;
    memset(image->color_components, 0, sizeof(image->color_components));
    memset(image->quantization_tables, 0, sizeof(image->quantization_tables));
    memset(image->huffman_dc_table, 0, sizeof(image->huffman_dc_table));
    memset(image->huffman_ac_tables, 0, sizeof(image->huffman_ac_tables));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: not enough arguments\n");
        fprintf(stderr, "Usage: %s <jpeg_file> [jpeg_file2 ...]\n", argv[0]);
        exit(1);
    }
    
    for (int i = 1; i < argc; ++i) {
        const char* filename = argv[i];
        printf("\n======================================\n");
        printf("Processing: %s\n", filename);
        printf("======================================\n\n");
        
        Image* image = read_JPEG(filename);
        
        if (image->valid) {
            printf("\n======================================\n");
            printf("Successfully decoded JPEG!\n");
            printf("Decoded %d blocks\n", image->block_height_real * image->block_width_real);
            printf("======================================\n\n");
            
            // Generate output filename
            const char* dot = strrchr(filename, '.');
            const char* slash = strrchr(filename, '/');
            if (slash == NULL) slash = filename - 1;
            
            char outfilename[256];
            if (dot && dot > slash) {
                size_t base_len = dot - filename;
                snprintf(outfilename, sizeof(outfilename), "%.*s_decoded.bmp", 
                        (int)base_len, filename);
            } else {
                snprintf(outfilename, sizeof(outfilename), "%s_decoded.bmp", filename);
            }
            
            // Write BMP file with raw DCT coefficients
            write_bmp(image, outfilename);
            
            // Also write DC-only thumbnail
            char dc_filename[256];
            if (dot && dot > slash) {
                size_t base_len = dot - filename;
                snprintf(dc_filename, sizeof(dc_filename), "%.*s_dc_only.bmp", 
                        (int)base_len, filename);
            } else {
                snprintf(dc_filename, sizeof(dc_filename), "%s_dc_only.bmp", filename);
            }
            write_bmp_dc_only(image, dc_filename);
            
        } else {
            printf("\n======================================\n");
            printf("Failed to decode JPEG.\n");
            printf("======================================\n");
        }
        
        if (image->blocks != NULL) {
            free(image->blocks);
        }
        free(image);
    }
    
    return 0;
}