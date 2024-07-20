# ea-pak
This repository is a companion to my blog post about the reverse engineering of the '.PAK' image file format used with *688 Attack Sub*, and possibly others, by *Electronic Arts (EA)*. For more detail please read [my blog post](https://canadianavenger.io/2024/07/15/attack-of-the-subs/). 

## EA Titles Known to Use the PAK Format
- 688 Attack Sub (1989)

## The Code

In this repo there are two C programs, each is a standalone utility for converting between the EA-PAK format and the Windows BMP format. The code is written to be portable, and should be able to be compiled for Windows, Linux, or Mac. The code is offered without warranty under the MIT License. Use it as you will personally or commercially, just give credit if you do.

- `pak2bmp.c` converts the given `.PAK` image into a Windows BMP format image
- `bmp2pak.c` converts the given Windows BMP format image into a `.PAK` image 

## The PAK File Format
The PAK Image File format is very basic and actually consists of 2 files. The first is the `.PAK` file itself, this holds the raw uncompressed 8 bit image data for a 320x200 image, for a total of 64000 bytes. The pixel data is stored in scanline order form the top left to the bottom right. The second is a `.PAL` file that holds the palette data for the image. The palette file is 256 entries of 8 bit red, green, and blue, component values, for a total of 768 bytes.

```c
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pal_entry_t;

typedef struct {
    pal_entry_t pal[256];
} pal_file_t;
```
 
