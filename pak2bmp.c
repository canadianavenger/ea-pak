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

#define OUTEXT   ".BMP"   // default extension for the output file
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
int save_bmp(const char *fn, memstream_buf_t *src, uint16_t width, uint16_t height, pal_entry_t *xpal);

int main(int argc, char *argv[]) {
    int rval = -1;
    FILE *fi = NULL;
    FILE *fp = NULL;
    char *fi_name = NULL;
    char *fp_name = NULL;
    char *fo_name = NULL;
    memstream_buf_t src = {0, 0, NULL};
    pal_entry_t     *pal = NULL;

    printf("Electronic Arts PAK file format to BMP converter\n");

    if((argc < 2) || (argc > 2)) {
        printf("USAGE: %s [infile]\n", filename(argv[0]));
        printf("[infile] is the name of the input PAK file to convert.\n");
        printf("the prgram expects an accompanying palette file of the\n");
        printf("same name as 'infile' except with a .PAL extension.\n");
        printf("output file will have the same name as 'infile', with .BMP extension\n");
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

    // open the input file
    printf("Opening PAK File: '%s'", fi_name);
    if(NULL == (fi = fopen(fi_name,"rb"))) {
        printf("Error: Unable to open input file\n");
        goto CLEANUP;
    }

    // determine size of the EXE file
    size_t fsi = filesize(fi);
    printf("\tFile Size: %zu\n", fsi);

    if((IMAGE_WIDTH * IMAGE_HEIGHT) != fsi) {
        printf("Error: Invalid Palette Size\n");
        goto CLEANUP;
    }

    // open the palette file
    printf("Opening PAL File: '%s'", fp_name);
    if(NULL == (fp = fopen(fp_name,"rb"))) {
        printf("Error: Unable to open palette file\n");
        goto CLEANUP;
    }

    // determine size of the EXE file
    size_t fsp = filesize(fp);
    printf("\tFile Size: %zu\n", fsp);

    if(768 != fsp) {
        printf("Error: Invalid Palette Size\n");
        goto CLEANUP;
    }

    // allocate image buffer
    if(NULL == (src.data = calloc(1, fsi))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    src.len = fsi;
    // allocate palette buffer
    if(NULL == (pal = calloc(1, fsp))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }

    // read in the image and palette
    if(1 != fread(src.data, fsi, 1, fi)) {
        printf("Error reading image\n");
        goto CLEANUP;
    }
    if(1 != fread(pal, fsp, 1, fp)) {
        printf("Error reading palette\n");
        goto CLEANUP;
    }

    if(0 != save_bmp(fo_name, &src, IMAGE_WIDTH, IMAGE_HEIGHT, pal)) {
        printf("Error Saving BMP file\n");
        goto CLEANUP;
    }
    
DONE:
    printf("Done\n");
    rval = 0; // clean exit

CLEANUP:
    fclose_s(fi);
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

// allocate a header buffer large enough for all 3 parts, plus 16 bit padding at the start to 
// maintian 32 bit alignment after the 16 bit signature.
#define HDRBUFSZ (sizeof(bmp_signature_t) + sizeof(bmp_header_t))

/// @brief saves the image pointed to by src as a BMP, assumes 256 colour 1 byte per pixel image data
/// @param fn name of the file to create and write to
/// @param src memstream buffer pointer to the source image data
/// @param width  width of the image in pixels
/// @param height height of the image in pixels or lines
/// @param pal pointer to 256 entry RGB palette
/// @return 0 on success, otherwise an error code
int save_bmp(const char *fn, memstream_buf_t *src, uint16_t width, uint16_t height, pal_entry_t *xpal) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer, also holds header info

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == src) || (NULL == src->data)) {
        rval = -1;  // NULL pointer error
        goto bmp_cleanup;
    }

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = -2;  // can't open/create output file
        goto bmp_cleanup;
    }

    // stride is the bytes per line in the BMP file, which are padded
    // out to 32 bit boundaries
    uint32_t stride = ((width + 3) & (~0x0003)); 
    uint32_t bmp_img_sz = (stride) * height;

    // allocate a buffer to hold the header and a single scanline of data
    // this could be optimized if necessary to only allocate the larger of
    // the line buffer, or the header + padding as they are used at mutually
    // exclusive times
    if(NULL == (buf = calloc(1, HDRBUFSZ + stride + 2))) {
        rval = -3;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // signature starts after padding to maintain 32bit alignment for the rest of the header
    bmp_signature_t *sig = (bmp_signature_t *)&buf[stride + 2];

    // bmp header starts after signature
    bmp_header_t *bmp = (bmp_header_t *)&buf[stride + 2 + sizeof(bmp_signature_t)];

    // setup the signature and DIB header fields
    *sig = BMPFILESIG;
    size_t palsz = sizeof(bmp_palette_entry_t) * 256;
    bmp->dib.image_offset = HDRBUFSZ + palsz;
    bmp->dib.file_size = bmp->dib.image_offset + bmp_img_sz;

    // setup the bmi header fields
    bmp->bmi.header_size = sizeof(bmi_header_t);
    bmp->bmi.image_width = width;
    bmp->bmi.image_height = height;
    bmp->bmi.num_planes = 1;           // always 1
    bmp->bmi.bits_per_pixel = 8;       // 256 colour image
    bmp->bmi.compression = 0;          // uncompressed
    bmp->bmi.bitmap_size = bmp_img_sz;
    bmp->bmi.horiz_res = BMP96DPI;
    bmp->bmi.vert_res = BMP96DPI;
    bmp->bmi.num_colors = 256;         // palette has 256 colours
    bmp->bmi.important_colors = 0;     // all colours are important

    // write out the header
    int nr = fwrite(sig, HDRBUFSZ, 1, fp);
    if(1 != nr) {
        rval = -4;  // unable to write file
        goto bmp_cleanup;
    }

    bmp_palette_entry_t *pal = calloc(256, sizeof(bmp_palette_entry_t));
    if(NULL == pal) {
        rval = -3;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // copy the external RGB palette to the BMP BGRA palette
    for(int i = 0; i < 256; i++) {
        pal[i].r = xpal[i].r;
        pal[i].g = xpal[i].g;
        pal[i].b = xpal[i].b;
    }

    // write out the palette
    nr = fwrite(pal, palsz, 1, fp);
    if(1 != nr) {
        rval = -4;  // can't write file
        goto bmp_cleanup;
    }
    // we can free the palette now as we don't need it anymore
    free(pal);

    // now we need to output the image scanlines. For maximum
    // compatibility we do so in the natural order for BMP
    // which is from bottom to top. 
    // start by pointing to start of last line of data
    uint8_t *px = &src->data[src->len - width];
    // loop through the lines
    for(int y = 0; y < height; y++) {
        memset(buf, 0, stride); // zero out the line in the output buffer
        // loop through all the pixels for a line
        for(int x = 0; x < width; x++) {
            buf[x] = *px++;
        }
        nr = fwrite(buf, stride, 1, fp); // write out the line
        if(1 != nr) {
            rval = -4;  // unable to write file
            goto bmp_cleanup;
        }
        px -= (width * 2); // move back to start of previous line
    }

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    return rval;
}
