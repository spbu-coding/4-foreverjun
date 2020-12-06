#include <stdio.h>
#include <stdint.h>
#include "qdbmp.h"
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


int read_and_check_header (uint32_t *header, FILE *input_file)
{
    long long real_file_size;
    unsigned int i;
    uint16_t file_format;
    if (fseek(input_file, 0, SEEK_END)) {
        error("fseek() error.");
        return -1;
    }
    real_file_size = ftell(input_file);
    if (real_file_size == -1L) {
        error("ftell() error.");
        return -1;
    }
    if (fseek(input_file, 0, SEEK_SET)) {
        error("fseek() error.");
        return -1;
    }
    if (fread(&file_format, sizeof(uint16_t), 1, input_file) != 1) {
        if (feof(input_file))
            error("Incorrect file. Empty file.");
        else
            error("File read error.");
        return -1;
    }
    if (file_format != 0x4d42) {
        error("Unsupported format.");
        return -1;
    }
    if (fread(header, sizeof(uint32_t), 13, input_file) !=13) {    //13 is the number of 4 bit cells in an array that contains the header data
        if (feof(input_file))
            error("Unsupported format.");
        else
            error("File read error.");
        return -1;
    }
    if (real_file_size != header[FILE_SIZE_A]) {
        error("Size data from metadata does not match the actual size.");
        return -2;
    }
    if (header[RESERVED_FIELDS_A] != 0) {
        error("Reserved fields should be equal to 0.");
        return -2;
    }
    if (header[DIB_HEADER_SIZE_A] != 40){
        error("Unsupported format. Only images with BITMAPINFOHEADER header name are supported.");
        return -2;
    }
    if ((header[FORMAT_A] << 16) != 0x10000){    //In one cell was written two 2bit values. that check their separate and used a bit shift.
        error("The number of color planes should be 1.");
        return -2;
    }
    if (((header[FORMAT_A] >> 16) != 24) && ((header[FORMAT_A] >> 16) != 8)){   //In one cell was written two 2bit values. that check their separate and used a bit shift.
        error("Unsupported format. Only 8-bit and 24-bit images are supported.");
        return -2;
    }
    if (header[COMPRESSION_A] != 0){
        error("Support only uncompressed images.");
        return -2;
    }
    if (((header[FORMAT_A] >> 16) == 8) && (header[NUMBER_OF_COLORS_IN_PALETTE_A] > 256)){
        error("Number of colors may not exceed 256.");
        return  -2;
    }
    if ((header[FORMAT_A] >> 16) == 24 &&
    ((header[WIDTH_A] % 4 + header[WIDTH_A] * 3) * abs ((signed)header[HEIGHT_A]) != (real_file_size - header[PIXEL_ARRAY_ADDRESS_A]))){
        error("The size of the character array does not coincide with the size specified in the header.");
        return -2;
    }
    if ((header[FORMAT_A] >> 16) == 8 &&
        (((4 - header[WIDTH_A] % 4) % 4 + header[WIDTH_A]) * abs ((signed)header[HEIGHT_A]) != (real_file_size - header[PIXEL_ARRAY_ADDRESS_A]))){
        error("The size of the character array does not coincide with the size specified in the header.");
        return -2;
    }
    if ((header[FORMAT_A] >> 16) == 8 &&
        (header[NUMBER_OF_COLORS_IN_PALETTE_A] * 4 != (header[PIXEL_ARRAY_ADDRESS_A] - HEADER_SIZE))){
        error("The size of the palette array does not coincide with the size specified in the header.");
        return -2;
    }
    return 0;
}


int convert_8bit_to_negative (FILE *input_file, uint32_t *header,char **argv) {
    FILE *output_file;
    uint8_t *palette, *pixels;
    uint16_t header_field = 0x4d42;
    unsigned int bytes_in_palette_arr = header[NUMBER_OF_COLORS_IN_PALETTE_A] * 4,
    bytes_in_pixel_arr = header[FILE_SIZE_A] - header[PIXEL_ARRAY_ADDRESS_A];
    if ((palette = calloc(bytes_in_palette_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        return -1;
    }
    if ((pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        free(palette);
        return -1;
    }
    if (fread(palette, sizeof(uint8_t), bytes_in_palette_arr, input_file) != bytes_in_palette_arr ) {
        free(pixels);
        free(palette);
        if (feof(input_file))
            error("Palette read error. End of file.");
        else
            error("Palette read error.");
        return -1;
    }
    if (fread(pixels, sizeof(uint8_t), bytes_in_pixel_arr, input_file) != bytes_in_pixel_arr ) {
        free(pixels);
        free(palette);
        if (feof(input_file))
            error("Pixel array read error. End of file.");
        else
            error("Pixel array read error.");
        return -1;
    }
    output_file = fopen(argv[3], "wb");
    for (int i = 0; i < bytes_in_palette_arr ; i += 4) {
        palette[i] = ~palette[i];//r
        palette[i + 1] = ~palette[i + 1];//g
        palette[i + 2] = ~palette[i + 2];//b
    }
    if (fwrite(&header_field, sizeof(uint16_t), 1, output_file) != 1) {
        error("Data writing error");
        free(pixels);
        free(palette);
        return -1;
    }
    if (fwrite(header, sizeof(uint8_t), HEADER_SIZE - 2, output_file) != HEADER_SIZE - 2) {
        error("Data writing error");
        free(palette);
        free(pixels);
        return -1;
    }
    if (fwrite(palette, sizeof(uint8_t), bytes_in_palette_arr, output_file) != bytes_in_palette_arr) {
        error("Data writing error");
        free(palette);
        free(pixels);
        return -1;
    }
    if (fwrite(pixels, sizeof(uint8_t), bytes_in_pixel_arr, output_file) != bytes_in_pixel_arr) {
        error("Data writing error");
        free(palette);
        free(pixels);
        return -1;
    }
    fclose(output_file);
    free(pixels);
    free(palette);
    return  0;
}


int convert_24bit_to_negative(FILE *input_file, uint32_t *header,char **argv)
{
    FILE *output_file;
    uint8_t *pixels;
    uint16_t header_field = 0x4d42;
    unsigned int bytes_in_pixel_arr = header[FILE_SIZE_A] - header[PIXEL_ARRAY_ADDRESS_A];
    int add_on_to_DWORD = header[WIDTH_A] % 4, m = 0;
    if ((pixels = calloc(bytes_in_pixel_arr, sizeof(uint8_t))) == NULL) {
        error("Memory allocation error.");
        return -1;
    }
    if (fread(pixels, sizeof(uint8_t), bytes_in_pixel_arr, input_file) != bytes_in_pixel_arr ) {
        free(pixels);
        if (feof(input_file))
            error("Pixel array read error. End of file.");
        else
            error("Pixel array read error.");
        return -1;
    }
    for (int i = 0; i < bytes_in_pixel_arr; i += 3) {
        pixels[i] = ~pixels[i];//r
        pixels[i + 1] = ~pixels[i + 1];//g
        pixels[i + 2] = ~pixels[i + 2];//b
        m++;
        if (header[WIDTH_A] == m){
            i += add_on_to_DWORD;
            m = 0;
        }
    }
    output_file = fopen(argv[3], "wb");
    if (fwrite(&header_field, sizeof(uint16_t), 1, output_file) != 1) {
        error("Data writing error");
        free(pixels);
        return -1;
    }
    if (fwrite(header, sizeof(uint8_t), HEADER_SIZE - 2, output_file) != HEADER_SIZE - 2) {
        error("Data writing error");
        free(pixels);
        return -1;
    }
    if (fwrite(pixels, sizeof(uint8_t), bytes_in_pixel_arr, output_file) != bytes_in_pixel_arr) {
        error("Data writing error");
        free(pixels);
        return -1;
    }
    fclose(output_file);
    free(pixels);
    return 0;
}


int convert_to_negative_qdbmp ( char **argv )
{
    UCHAR	r, g, b;
    UINT	width, height;
    UINT	x, y;
    BMP*	bmp;
    /* Read an image file */
    bmp = BMP_ReadFile( argv[ 2 ] );
    BMP_CHECK_ERROR( stderr, -1 );
    printf("3");
    /* Get image's dimensions */
    width = BMP_GetWidth( bmp );
    height = BMP_GetHeight( bmp );
    /* Iterate through all the image's pixels */
    for ( x = 0 ; x < width ; ++x )
    {
        for ( y = 0 ; y < height ; ++y )
        {
            /* Get pixel's RGB values */
            BMP_GetPixelRGB( bmp, x, y, &r, &g, &b );

            /* Invert RGB values */
            BMP_SetPixelRGB( bmp, x, y, ~r, ~g, ~b );
        }
    }
    /* Save result */
    BMP_WriteFile( bmp, argv[ 3 ] );
    BMP_CHECK_ERROR( stderr, -1 );
    /* Free all memory allocated for the image */
    BMP_Free( bmp );
    return 0;
}
int main(int argc, char *argv[])
{
    uint32_t header[13]; //13 is the number of 4 bit cells in an array that contains the header data
    FILE *input_file;
    int result;
    if(argc != 4  || (strcmp(argv[1], "--mine") && strcmp(argv[1], "--theirs"))){
        error("You must enter 3 arguments with a space:\n1.'--mine' or '--theirs' (this argument should be the first)\n2.<input_file>.bmp\n3.<output_file>.bmp");
        return -1;
    }
    if ((input_file = fopen(argv[2], "rb")) == NULL){
        error("File not found");
        return -1;
    }
    result = read_and_check_header(header, input_file);
    if (result != 0)
        return result;
    if (!strcmp(argv[1], "--theirs")){
        if ((signed)header[HEIGHT_A] < 0){
            error("qdbmp library not support negative height images. Use --mine option ");
            return -2;
        }
        fclose(input_file);
        if(convert_to_negative_qdbmp(argv))
            return -3;
    }
    else {
        if ((header[FORMAT_A] >> 16) == 8){
            if ((result = convert_8bit_to_negative(input_file,header,argv)) != 0)
                return result;
        }
        if ((header[FORMAT_A] >> 16) == 24){
            if ((result = convert_24bit_to_negative(input_file,header,argv)) != 0)
                return result;
        }
    }
    fclose(input_file);
    return 0;
}
