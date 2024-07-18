/*
 * pak2bmp.c 
 * Converts a given EA-PAK to a Windows BMP file
 *  
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bmp.h"

#define OUTEXT   ".PAK"   // default extension for the output file
#define PALEXT   ".PAL"   // default extension for the palette file
#define IMAGE_WIDTH (320)
#define IMAGE_HEIGHT (200)

typedef struct {
    size_t      len;         // length of buffer in bytes
    size_t      pos;         // current byte position in buffer
    uint8_t     *data;       // pointer to bytes in memory
} memstream_buf_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pal_entry_t;

size_t filesize(FILE *f);
void drop_extension(char *fn);
char *filename(char *path);
#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL
int load_bmp(memstream_buf_t *dst, const char *fn, uint16_t *width, uint16_t *height, pal_entry_t *xpal);

int main(int argc, char *argv[]) {
    int rval = -1;
    FILE *fp = NULL;
    FILE *fo = NULL;
    char *fi_name = NULL;
    char *fp_name = NULL;
    char *fo_name = NULL;
    memstream_buf_t src = {0, 0, NULL};
    pal_entry_t     *pal = NULL;
    uint16_t width = 0;
    uint16_t height = 0;

    printf("BMP to Electronic Arts PAK file format converter\n");

    if((argc < 2) || (argc > 2)) {
        printf("USAGE: %s [infile]\n", filename(argv[0]));
        printf("[infile] is the name of the input BMP file to convert.\n");
        printf("the prgram expects an accompanying palette file of the\n");
        printf("same name as 'infile' except with a .PAL extension.\n");
        printf("output file will have the same name as 'infile', with .PAK extension\n");
        return -1;
    }
    argv++; argc--; // consume the first arg (program name)

    // get the file name from the command line
    int namelen = strlen(argv[0]);
    if(NULL == (fi_name = calloc(1, namelen+1))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    if(NULL == (fo_name = calloc(1, namelen+5))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    if(NULL == (fp_name = calloc(1, namelen+5))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    strncpy(fi_name, argv[0], namelen);
    // generate the other file names from the input name
    strncpy(fo_name, argv[0], namelen);
    strncpy(fp_name, argv[0], namelen);
    drop_extension(fo_name);
    drop_extension(fp_name);
    strncat(fo_name, OUTEXT, namelen + 4);
    strncat(fp_name, PALEXT, namelen + 4);

    if(NULL == (pal = calloc(256, sizeof(pal_entry_t)))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }

    printf("Loading BMP File: '%s'\n", fi_name);
    if(0 != load_bmp(&src, fi_name, &width, &height, pal)) {
        printf("Error loading BMP image\n");
        goto CLEANUP;
    }

    printf("Saving PAK File: '%s'\n", fo_name);
    // try to open/create output file
    if(NULL == (fo = fopen(fo_name, "wb"))) {
        printf("Error creating image file\n");
        goto CLEANUP;
    }
    fwrite(src.data, src.len, 1, fo);
    fclose_s(fo);

    printf("Saving PAL File: '%s'\n", fp_name);
    if(NULL == (fp = fopen(fp_name, "wb"))) {
        printf("Error creating palette file\n");
        goto CLEANUP;
    }
    fwrite(pal, sizeof(pal_entry_t), 256, fp);
    fclose_s(fp);
    
DONE:
    printf("Done\n");
    rval = 0; // clean exit

CLEANUP:
    fclose_s(fo);
    fclose_s(fp);
    free_s(src.data);
    free_s(pal);
    free_s(fi_name);
    free_s(fo_name);
    free_s(fp_name);
    return rval;
}

/// @brief determins the size of the file
/// @param f handle to an open file
/// @return returns the size of the file
size_t filesize(FILE *f) {
    size_t szll, cp;
    cp = ftell(f);           // save current position
    fseek(f, 0, SEEK_END);   // find the end
    szll = ftell(f);         // get positon of the end
    fseek(f, cp, SEEK_SET);  // restore the file position
    return szll;             // return position of the end as size
}

/// @brief removes the extension from a filename
/// @param fn sting pointer to the filename
void drop_extension(char *fn) {
    char *extension = strrchr(fn, '.');
    if(NULL != extension) *extension = 0; // strip out the existing extension
}

/// @brief Returns the filename portion of a path
/// @param path filepath string
/// @return a pointer to the filename portion of the path string
char *filename(char *path) {
	int i;

	if(path == NULL || path[0] == '\0')
		return "";
	for(i = strlen(path) - 1; i >= 0 && path[i] != '/'; i--);
	if(i == -1)
		return "";
	return &path[i+1];
}

/// @brief loads the BMP image from a file, assumes 256 colour image.
/// @param dst pointer to a empty memstream buffer struct. load_bmp will allocate the buffer, image will be stored as 1 byte per pixel
/// @param fn name of file to load
/// @param width  pointer to width of the image in pixels set on return
/// @param height pointer to height of the image in pixels or lines set on return
/// @param pal pointer to a 256 entry RGB palette, will be overwritten with palette from file
/// @return  0 on success, otherwise an error code
int load_bmp(memstream_buf_t *dst, const char *fn, uint16_t *width, uint16_t *height, pal_entry_t *xpal) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer
    bmp_header_t *bmp = NULL;
    bmp_palette_entry_t *pal = NULL;

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == dst) || (NULL == width) || (NULL == height)) {
        rval = -1;  // NULL pointer error
        goto bmp_cleanup;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = -2;  // can't open input file
        goto bmp_cleanup;
    }

    bmp_signature_t sig = 0;
    int nr = fread(&sig, sizeof(bmp_signature_t), 1, fp);
    if(1 != nr) {
        rval = -3;  // unable to read file
        goto bmp_cleanup;
    }
    if(BMPFILESIG != sig) {
        rval = -4; // not a BMP file
        goto bmp_cleanup;
    }

    // allocate a buffer to hold the header 
    if(NULL == (bmp = calloc(1, sizeof(bmp_header_t) + (256 * sizeof(bmp_palette_entry_t))))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }
    nr = fread(bmp, sizeof(bmp_header_t), 1, fp);
    if(1 != nr) {
        rval = -3;  // unable to read file
        goto bmp_cleanup;
    }

    // check some basic header vitals to make sure it's in a format we can work with
    if((1 != bmp->bmi.num_planes) || 
       (sizeof(bmi_header_t) != bmp->bmi.header_size) || 
       (0 != bmp->dib.RES)) {
        rval = -6;  // invalid header
        goto bmp_cleanup;
    }
    if((8 != bmp->bmi.bits_per_pixel) ||  
       (0 != bmp->bmi.compression)) {
        rval = -7;  // unsupported BMP format
        goto bmp_cleanup;
    }
    
    // allocate a buffer to hold the palette 
    if(NULL == (pal = calloc(bmp->bmi.num_colors, sizeof(bmp_palette_entry_t)))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }
    nr = fread(pal, sizeof(bmp_palette_entry_t), bmp->bmi.num_colors, fp);
    if(bmp->bmi.num_colors != nr) {
        rval = -3;  // unable to read file
        goto bmp_cleanup;
    }

    // read in and translate the palette to RGB
    for(int i = 0; i < bmp->bmi.num_colors; i++) {
        xpal[i].r = pal[i].r;
        xpal[i].g = pal[i].g;
        xpal[i].b = pal[i].b;
    }

    // check if the destination buffer is null, if not, free it
    // we will allocate it ourselves momentarily
    if(NULL != dst->data) {
        free(dst->data);
        dst->data = NULL;
    }

    // if height is negative, flip the render order
    bool flip = (bmp->bmi.image_height < 0); 
    bmp->bmi.image_height = abs(bmp->bmi.image_height);

    uint16_t lw = bmp->bmi.image_width;
    uint16_t lh = bmp->bmi.image_height;

    // stride is the bytes per line in the BMP file, which are padded
    uint32_t stride = ((lw + 3) & (~0x0003)); 

    // allocate our line and output buffers
    if(NULL == (dst->data = calloc(1, lw * lh))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }
    dst->len = lw * lh;
    dst->pos = 0;

    if(NULL == (buf = calloc(1, stride))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // now we need to read the image scanlines. 
    // start by pointing to start of last line of data
    uint8_t *px = &dst->data[dst->len - lw]; 
    if(flip) px = dst->data; // if flipped, start at beginning
    // loop through the lines
    for(int y = 0; y < lh; y++) {
        nr = fread(buf, stride, 1, fp); // read a line
        if(1 != nr) {
            rval = -3;  // unable to read file
            goto bmp_cleanup;
        }

        for(int x = 0; x < lw; x++) {
            *px++ = buf[x];
        }

        if(!flip) { // if not flipped, wehave to walk backwards
            px -= (lw * 2); // move back to start of previous line
        }
    }

    *width = lw;
    *height = lh;

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    free_s(bmp);
    free_s(pal);
    return rval;
}