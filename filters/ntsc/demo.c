/* Displays and saves NTSC filtered image. Mouse controls sharpness and gamma.
Defaults to using "test.bmp" for input and "filtered.bmp" for output. Input
image must be an uncompressed BMP.

Usage: demo [in.bmp [out.bmp]]

Space   Toggle field merging
C       Composite video quality
S       S-video quality
R       RGB video quality
M       Monochrome video quality
D       Toggle between standard and Sony decoder matrix
*/

#include "snes_ntsc.h"

#include "demo_impl.h"

int main( int argc, char** argv )
{
	image_t image;
	int sony_decoder = 0;
	int merge_fields = 1;
	int burst_phase = 0;
	snes_ntsc_setup_t setup = snes_ntsc_composite;
	
	snes_ntsc_t* ntsc = (snes_ntsc_t*) malloc( sizeof (snes_ntsc_t) );
	if ( !ntsc )
		fatal_error( "Out of memory" );
	snes_ntsc_init( ntsc, &setup );
	
	load_bmp( &image, (argc > 1 ? argv [1] : "test.bmp"), 0 );
	init_window( SNES_NTSC_OUT_WIDTH( image.width ), image.height * 2 );
	
	while ( read_input() )
	{
		lock_pixels();
		
		burst_phase ^= 1;
		if ( setup.merge_fields )
			burst_phase = 0;
		
		snes_ntsc_blit( ntsc, image.rgb_16, image.row_width, burst_phase,
				image.width, image.height, output_pixels, output_pitch );
		
		double_output_height();
		display_output();
		
		switch ( key_pressed )
		{
			case ' ': merge_fields = !merge_fields; break;
			case 'c': setup = snes_ntsc_composite; break;
			case 's': setup = snes_ntsc_svideo; break;
			case 'r': setup = snes_ntsc_rgb; break;
			case 'm': setup = snes_ntsc_monochrome; break;
			case 'd': sony_decoder = !sony_decoder; break;
		}
		
		if ( key_pressed || mouse_moved )
		{
			setup.merge_fields = merge_fields;
			
			/* available parameters: hue, saturation, contrast, brightness,
			sharpness, gamma, bleed, resolution, artifacts, fringing */
			setup.sharpness = mouse_x;
			setup.gamma     = mouse_y;
			
			setup.decoder_matrix = 0;
			if ( sony_decoder )
			{
				/* Sony CXA2025AS US */
				static float matrix [6] = { 1.630, 0.317, -0.378, -0.466, -1.089, 1.677 };
				setup.decoder_matrix = matrix;
			}
			
			snes_ntsc_init( ntsc, &setup );
		}
	}
	
	save_bmp( argc > 2 ? argv [2] : "filtered.bmp" );
	
	free( ntsc );
	
	return 0;
}
