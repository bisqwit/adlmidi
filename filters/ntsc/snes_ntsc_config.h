/* Configure library by modifying this file */

#ifndef SNES_NTSC_CONFIG_H
#define SNES_NTSC_CONFIG_H

/* Format of source pixels */
/* #define SNES_NTSC_IN_FORMAT SNES_NTSC_RGB16 */
/* #define SNES_NTSC_IN_FORMAT SNES_NTSC_BGR15 */
#define SNES_NTSC_IN_FORMAT SNES_NTSC_RGB24
/* #define SNES_NTSC_IN_FORMAT SNES_NTSC_BGR24 */

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Bits per pixel of output. Can be 15, 16, 32, or 24 (same as 32). */
#define SNES_NTSC_OUT_DEPTH 24

/* Type of input pixel values */
struct snes_ntsc_in_t { unsigned char bytes[3]; } __attribute__((packed));
#define SNES_NTSC_IN_T snes_ntsc_in_t

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define SNES_NTSC_ADJ_IN( in ) ((in).bytes[0] + (in).bytes[1]*0x100u + (in).bytes[2]*0x10000u)

/* For each pixel, this is the basic operation:
output_color = SNES_NTSC_ADJ_IN( SNES_NTSC_IN_T ) */

#endif
