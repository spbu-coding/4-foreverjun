#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define error(...) (fprintf(stderr, __VA_ARGS__))
//All macros marked A means the address of the parameter from the header in the array
#define FILE_SIZE_A     0
#define RESERVED_FIELDS_A     1
#define PIXEL_ARRAY_ADDRESS_A     2
#define DIB_HEADER_SIZE_A     3
#define WIDTH_A     4
#define HEIGHT_A     5
#define FORMAT_A     6
#define COMPRESSION_A     7
#define NUMBER_OF_COLORS_IN_PALETTE_A     11

#define HEADER_SIZE 0x36

int read_and_check_header (uint32_t *header, FILE *input_file,char *file_name)
{
    unsigned int real_file_size;
    uint16_t file_format;
    if (fseek(input_file, 0, SEEK_END)) {
        error("fseek() error. File: %s", file_name);
        return -1;
    }
    if ((real_file_size = ftell(input_file)) == -1L) {
        error("ftell() error. File: %s", file_name);
        return -1;
    }
    if (fseek(input_file, 0, SEEK_SET)) {
        error("fseek() error. File: %s", file_name);
        return -1;
    }
    if (fread(&file_format, sizeof(uint16_t), 1, input_file) != 1) {
        if (feof(input_file))
            error("Incorrect file. Empty file. File: %s", file_name);
        else
            error("File read error. File: %s", file_name);
        return -1;
    }
    if (file_format != 0x4d42) {
        error("Unsupported format. File: %s", file_name);
        return -1;
    }
    if (fread(header, sizeof(uint32_t), 13, input_file) !=13) {    //13 is the number of 4 bit cells in an array that contains the header data
        if (feof(input_file))
            error("Unsupported format. File: %s", file_name);
        else
            error("File read error. File: %s", file_name);
        return -1;
    }
    if (real_file_size != header[FILE_SIZE_A]) {
        error("Size data from metadata does not match the actual size. File: %s", file_name);
        return -2;
    }
    if (header[RESERVED_FIELDS_A] != 0) {
        error("Reserved fields should be equal to 0. File: %s", file_name);
        return -2;
    }
    if (header[DIB_HEADER_SIZE_A] != 40){
        error("Unsupported format. Only images with BITMAPINFOHEADER header name are supported. File: %s", file_name);
        return -2;
    }
    if ((header[FORMAT_A] << 16) != 0x10000){    //In one cell was written two 2bit values. that check their separate and used a bit shift.
        error("The number of color planes should be 1. File: %s", file_name);
        return -2;
    }
    if (((header[FORMAT_A] >> 16) != 24) && ((header[FORMAT_A] >> 16) != 8)){   //In one cell was written two 2bit values. that check their separate and used a bit shift.
        error("Unsupported format. Only 8-bit and 24-bit images are supported. File: %s", file_name);
        return -2;
    }
    if (header[COMPRESSION_A] != 0){
        error("Support only uncompressed images. File: %s", file_name);
        return -2;
    }
    if (((header[FORMAT_A] >> 16) == 8) && (header[NUMBER_OF_COLORS_IN_PALETTE_A] > 256)){
        error("Number of colors may not exceed 256. File: %s", file_name);
        return  -2;
    }
    if ((header[FORMAT_A] >> 16) == 24 &&
        ((header[WIDTH_A] % 4 + header[WIDTH_A] * 3) * abs ((signed)header[HEIGHT_A]) != (real_file_size - header[PIXEL_ARRAY_ADDRESS_A]))){
        error("The size of the character array does not coincide with the size specified in the header. File: %s", file_name);
        return -2;
    }
    if ((header[FORMAT_A] >> 16) == 8 &&
        (((4 - header[WIDTH_A] % 4) % 4 + header[WIDTH_A]) * abs ((signed)header[HEIGHT_A]) != (real_file_size - header[PIXEL_ARRAY_ADDRESS_A]))){
        error("The size of the character array does not coincide with the size specified in the header. File: %s", file_name);
        return -2;
    }
    if ((header[FORMAT_A] >> 16) == 8 &&
        (header[NUMBER_OF_COLORS_IN_PALETTE_A] * 4 != (header[PIXEL_ARRAY_ADDRESS_A] - HEADER_SIZE))){
        error("The size of the palette array does not coincide with the size specified in the header. File: %s", file_name);
        return -2;
    }
    return 0;
}

int compare_8bit (uint32_t *first_header, FILE *first_input_file, uint32_t *second_header, FILE *second_input_file)
{
    unsigned int bytes_in_first_palette_arr = first_header[NUMBER_OF_COLORS_IN_PALETTE_A] * 4,
        bytes_in_pixel_arr = first_header[FILE_SIZE_A] - first_header[PIXEL_ARRAY_ADDRESS_A],
        bytes_in_second_palette_arr = first_header[NUMBER_OF_COLORS_IN_PALETTE_A] * 4;
    long x = 0, y = 0;
    long long i, m = 0, extension_to_DWORD32 = (4 - first_header[WIDTH_A] % 4) % 4;
    uint8_t *first_palette, *second_palette, *first_pixels, *second_pixels;
    if (first_header[WIDTH_A] != second_header[WIDTH_A] ||
    abs((signed)first_header[HEIGHT_A]) != abs((signed)second_header[HEIGHT_A])){
        error("The linear dimensions of the images do not coincide");
        return -1;
    }
    if ((first_palette = calloc(bytes_in_first_palette_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        return -1;
    }
    if ((first_pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        free(first_palette);
        return -1;
    }
    if ((second_palette = calloc(bytes_in_second_palette_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        free(first_palette);
        free(first_pixels);
        return -1;
    }
    if ((second_pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        free(first_palette);
        free(first_pixels);
        free(second_palette);
        return -1;
    }
    if (fread(first_palette, sizeof(uint8_t), bytes_in_first_palette_arr, first_input_file) != bytes_in_first_palette_arr){
        free(first_palette);
        free(first_pixels);
        free(second_palette);
        free(second_pixels);
        if (feof(first_input_file))
            error("Palette read error. End of file.");
        else
            error("Palette read error.");
        return -1;
    }
    if (fread(first_pixels, sizeof(uint8_t), bytes_in_pixel_arr, first_input_file) != bytes_in_pixel_arr) {
        free(first_palette);
        free(first_pixels);
        free(second_palette);
        free(second_pixels);
        if (feof(first_input_file))
            error("Pixel read error. End of file.");
        else
            error("Pixel read error.");
        return -1;
    }
    if (fread(second_palette, sizeof(uint8_t), bytes_in_second_palette_arr, second_input_file) != bytes_in_second_palette_arr) {
        free(first_palette);
        free(first_pixels);
        free(second_palette);
        free(second_pixels);
        if (feof(second_input_file))
            error("Palette array read error. End of file.");
        else
            error("Palette array read error.");
        return -1;
    }
    if (fread(second_pixels, sizeof(uint8_t), bytes_in_pixel_arr, second_input_file) != bytes_in_pixel_arr) {
        free(first_palette);
        free(first_pixels);
        free(second_palette);
        free(second_pixels);
        if (feof(second_input_file))
            error("Pixel array read error. End of file.");
        else
            error("Pixel array read error.");
        return -1;
    }
    fclose(first_input_file);
    fclose(second_input_file);
    for (i = 0; i < bytes_in_pixel_arr ; i++) {
        if ((first_palette[first_pixels[i]] != second_palette[second_pixels[i]]) && m <= 100){
            fprintf(stderr, "(%ld , %ld)\n", x, y);
            m ++;
        }
        if (x == first_header[WIDTH_A]){
            x = 0;
            i += extension_to_DWORD32;
            y++;
        }
        x++;
    }
    free(first_palette);
    free(first_pixels);
    free(second_palette);
    free(second_pixels);
    if (m == 0)
        return 0;
    else
        return 1;
}

int compare_24bit (uint32_t *first_header, FILE *first_input_file, uint32_t *second_header, FILE *second_input_file)
{
    unsigned int bytes_in_pixel_arr = first_header[FILE_SIZE_A] - first_header[PIXEL_ARRAY_ADDRESS_A];
    long x = 0, y = 0;
    long long i, m = 0, extension_to_DWORD32 = first_header[WIDTH_A] % 4;
    uint8_t *first_pixels, *second_pixels;
    if (first_header[WIDTH_A] != second_header[WIDTH_A] ||
        abs((signed)first_header[HEIGHT_A]) != abs((signed)second_header[HEIGHT_A])){
        error("The linear dimensions of the images do not coincide");
        return -1;
    }
    if ((first_pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        return -1;
    }
    if ((second_pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        free(first_pixels);
        error("Memory allocation error.");
        return -1;
    }

    if (fseek(first_input_file, first_header[PIXEL_ARRAY_ADDRESS_A], SEEK_SET)) {
        free(first_pixels);
        free(second_pixels);
        error("fseek() error.");
        return -1;
    }
    if (fseek(second_input_file, second_header[PIXEL_ARRAY_ADDRESS_A], SEEK_SET)) {
        free(first_pixels);
        free(second_pixels);
        error("fseek() error.");
        return -1;
    }
    if (fread(first_pixels, sizeof(uint8_t), bytes_in_pixel_arr, first_input_file) != bytes_in_pixel_arr) {
        free(first_pixels);
        free(second_pixels);
        if (feof(first_input_file))
            error("Pixel read error. End of file.");
        else
            error("Pixel read error.");
        return -1;
    }
    if (fread(second_pixels, sizeof(uint8_t), bytes_in_pixel_arr, second_input_file) != bytes_in_pixel_arr) {
        free(first_pixels);
        free(second_pixels);
        if (feof(second_input_file))
            error("Pixel array read error. End of file.");
        else
            error("Pixel array read error.");
        return -1;
    }
    fclose(first_input_file);
    fclose(second_input_file);
    for (i = 0; i < bytes_in_pixel_arr ; i+=3) {
        if ((first_pixels[i] != second_pixels[i] || first_pixels[i + 1] != second_pixels[i + 1]
            || first_pixels[i + 2] != second_pixels[i + 2]) && m <= 100){
            fprintf(stderr, "(%ld , %ld)\n", x, y);
            m ++;
        }
        if (x == first_header[WIDTH_A]){
            x = 0;
            i += extension_to_DWORD32;
            y++;
        }
        x++;
    }
    free(first_pixels);
    free(second_pixels);
    if (m == 0)
        return 0;
    else
        return 1;
}

int main(int argc, char *argv[]){
    uint32_t first_header[13], second_header[13]; //13 is the number of 4 bit cells in an array that contains the header data
    FILE *first_input_file, *second_input_file;
    int result;
    if(argc != 3 ){
        error("You must enter the names of the two spanning files:\n1.<input_file>.bmp\n2.<input_file>.bmp");
        return -2;
    }
    if ((first_input_file = fopen(argv[1], "rb")) == NULL){
        error("%s not found", argv[1]);
        return -2;
    }
    if ((second_input_file = fopen(argv[2], "rb")) == NULL){
        error("%s not found", argv[2]);
        return -1;
    }
    if ((result = read_and_check_header(first_header, first_input_file, &*argv[1])) != 0)
        return result;
    if ((result = read_and_check_header(second_header, second_input_file, &*argv[2])) != 0)
        return result;
    if ((first_header[FORMAT_A] >> 16) == (second_header[FORMAT_A] >> 16) && (second_header[FORMAT_A] >> 16) == 8){
        if ((result = compare_8bit(first_header, first_input_file, second_header, second_input_file)) != 0)
            return result;
    }
    else{
        if ((first_header[FORMAT_A] >> 16) == (second_header[FORMAT_A] >> 16) && (second_header[FORMAT_A] >> 16) == 24){
            if ((result = compare_24bit(first_header, first_input_file, second_header, second_input_file)) != 0)
                return result;
        }
        else{
            error("Files have different bits. 8bit and 24bit");
        }
    }
        return 0;
}