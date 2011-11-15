#ifndef __MDXMINI_H__
#define __MDXMINI_H__

#include "mdx.h"
#include "pcm8.h"

typedef struct
{
	int samples;
	int channels;
	MDX_DATA *mdx;
	PDX_DATA *pdx;
	void *self;
} t_mdxmini;

#define MDX_FREQ (PCM8_MASTER_PCM_RATE)

void mdx_set_rate( int freq );
void mdx_set_dir ( t_mdxmini *data , char  * dir );
int  mdx_open( t_mdxmini *data , char *filename , char *pcmdir );
void mdx_disp_info(t_mdxmini *data);

int  mdx_next_frame ( t_mdxmini *data );
int  mdx_frame_length ( t_mdxmini *data );
void mdx_make_buffer( short *buf , int buffer_size);
int  mdx_calc_sample( t_mdxmini *data, short *buf , int buffer_size );

void  mdx_get_title( t_mdxmini *data, char *title );
int   mdx_get_length( t_mdxmini *data );
void  mdx_set_max_loop(t_mdxmini *data , int loop);

void mdx_stop( t_mdxmini *data );
int  mdx_get_buffer_size( void );
int  mdx_get_sample_size ( void );


int  mdx_get_tracks ( t_mdxmini *data );
void mdx_get_current_notes ( t_mdxmini *data , int *notes , int len );


#endif
