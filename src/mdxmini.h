#ifndef __MDXMINI_H__
#define __MDXMINI_H__

typedef struct
{
	void* mdx2151;
	void* mdxmml_ym2151;
	void* pcm8;
	void* ym2151_c;

} songdata;

#include "mdx.h"

typedef struct
{
	int samples;
	int channels;
	MDX_DATA *mdx;
	PDX_DATA *pdx;
	void *self;
	songdata *songdata;
    int nlg_tempo;

} t_mdxmini;

#include "pcm8.h"

#define MDX_FREQ (PCM8_MASTER_PCM_RATE)

void mdx_set_rate(int freq);
void mdx_set_dir(t_mdxmini *data, char *dir);
int  mdx_open(t_mdxmini *data, char *filename, char *pcmdir);
void mdx_close(t_mdxmini *data);
void mdx_disp_info(t_mdxmini *data);

int  mdx_next_frame(t_mdxmini *data);
int  mdx_frame_length(t_mdxmini *data);
void mdx_make_buffer(t_mdxmini *data, short *buf, int buffer_size);
int  mdx_calc_sample(t_mdxmini *data, short *buf, int buffer_size);
int  mdx_calc_log(t_mdxmini *data, short *buf, int buffer_size);

void  mdx_get_title(t_mdxmini *data, char *title);
int   mdx_get_length(t_mdxmini *data);
void  mdx_set_max_loop(t_mdxmini *data, int loop);

int  mdx_get_buffer_size(t_mdxmini *data);
int  mdx_get_sample_size(t_mdxmini *data);

int  mdx_get_tracks(t_mdxmini *data);
void mdx_get_current_notes(t_mdxmini *data, int *notes, int len);

#endif
