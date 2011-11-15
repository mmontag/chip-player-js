/*
  MDX player

  Made by Daisuke Nagano <breeze.nagano@nifty.ne.jp>
  Jan.16.1999

  independent from U*IX environment version by BKC
  Apr.??.2009

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "mdxmini.h"
#include "class.h"

/* ------------------------------------------------------------------ */
#define PATH_BUF_SIZE 1024

static PDX_DATA* _get_pdx(MDX_DATA* mdx, char* mdxpath);
static int self_construct(void);
static void self_destroy(void);

static void usage( void );
static void display_version( void );

// static char mdx_path[1024];

/* ------------------------------------------------------------------ */
// static char *command_name;
static char *pdx_pathname;
static char *dsp_device;

static int   no_pdx;
static int   no_fm;
static int   no_opl3;
static int   no_ym2151;
static int   no_fm_voice;
static int   fm_waveform;
static int   pcm_volume;
static int   fm_volume;
static int   volume;
static int   max_infinite_loops;
static int   fade_out_speed;
static int   dump_voice;
static int   output_titles;

static int   dsp_speed = 44100;


static int   is_use_fragment;
static int   is_output_to_stdout;
static int   is_output_to_stdout_in_wav;
static int   is_use_reverb;
static float reverb_predelay;
static float reverb_roomsize;
static float reverb_damp;
static float reverb_width;
static float reverb_dry;
static float reverb_wet;

/* ------------------------------------------------------------------ */

int mdx_open( t_mdxmini *data, char *filename , char *pcmdir )
{

  MDX_DATA *mdx = NULL;
  PDX_DATA *pdx = NULL;

  pdx_pathname        = (char *)NULL;
  no_pdx              = FLAG_FALSE;
  no_fm               = FLAG_FALSE;
  no_opl3             = FLAG_TRUE;
  no_ym2151           = FLAG_FALSE;
  no_fm_voice         = FLAG_FALSE;
  fm_waveform         = 0;
  pcm_volume          = 127;
  fm_volume           = 127;
  volume              = 127;
  is_output_to_stdout = FLAG_FALSE;
  max_infinite_loops  = 3;
  fade_out_speed      = 5;
  dump_voice          = FLAG_FALSE;
  output_titles       = FLAG_FALSE;
  is_use_reverb       = FLAG_FALSE;
  reverb_predelay     = 0.05f;
  reverb_roomsize     = 0.5f;
  reverb_damp         = 0.1f;
  reverb_width        = 0.8f;
  reverb_wet          = 0.2f;
  reverb_dry          = 0.5f;

  dsp_device          = (char *)NULL;
  is_use_fragment     = FLAG_TRUE;

  is_output_to_stdout_in_wav = FLAG_TRUE;


  if (!self_construct()) {
    /* failed to create class instances */
    return -1;
  }
    /* load mdx file */

    data->mdx = mdx_open_mdx( filename );
    if ( !data->mdx ) 
		return -1;
		
	mdx = data->mdx;
	
	if ( pcmdir )
		strcpy(mdx->pdx_dir , pcmdir );
	

    mdx->is_use_pcm8         = no_pdx      == FLAG_TRUE ? FLAG_FALSE:FLAG_TRUE;
    mdx->is_use_fm           = no_fm       == FLAG_TRUE ? FLAG_FALSE:FLAG_TRUE;
    mdx->is_use_opl3         = no_opl3     == FLAG_TRUE ? FLAG_FALSE:FLAG_TRUE;
    mdx->is_use_ym2151       = no_ym2151   == FLAG_TRUE ? FLAG_FALSE:FLAG_TRUE;
    mdx->is_use_fm_voice     = no_fm_voice == FLAG_TRUE ? FLAG_FALSE:FLAG_TRUE;
    mdx->fm_wave_form        = fm_waveform;
    mdx->master_volume       = volume;
    mdx->fm_volume           = fm_volume  * volume/127;
    mdx->pcm_volume          = pcm_volume * volume/127;
    mdx->max_infinite_loops  = max_infinite_loops;
    mdx->fade_out_speed      = fade_out_speed;

    mdx->is_output_to_stdout = is_output_to_stdout; 
    mdx->is_use_fragment     = is_use_fragment;
    mdx->dsp_device          = dsp_device;
    mdx->dump_voice          = dump_voice;
    mdx->is_output_titles    = output_titles;
	mdx->dsp_speed           = dsp_speed;

    mdx->is_use_reverb       = is_use_reverb;
    mdx->reverb_predelay     = reverb_predelay;
    mdx->reverb_roomsize     = reverb_roomsize;
    mdx->reverb_damp         = reverb_damp;
    mdx->reverb_width        = reverb_width;
    mdx->reverb_dry          = reverb_dry;
    mdx->reverb_wet          = reverb_wet;

    mdx->is_output_to_stdout_in_wav = is_output_to_stdout_in_wav; 

    /* voice data load */

    if ( mdx_get_voice_parameter( mdx ) != 0 )
		return -1;
	
    /* load pdx data */
    pdx = data->pdx = _get_pdx( mdx, filename );

	data->self = mdx_parse_mml_ym2151_async_initialize(mdx, pdx);

	if (!data->self)
			return -1;
			
	data->samples = 0;
	data->channels = pcm8_get_output_channels();

	return 0;

}

void mdx_set_dir ( t_mdxmini *data , char  * dir )
{
	strcpy(data->mdx->pdx_dir, dir );
}

void mdx_set_rate( int freq )
{
	dsp_speed = freq;
}

void mdx_set_max_loop(t_mdxmini *data , int loop)
{
	data->mdx->max_infinite_loops = loop;
}

void mdx_disp_info(t_mdxmini *data)
{
    /* output Title, etc... */

    if ( data->mdx->is_output_titles == FLAG_TRUE ) 
	{
      mdx_output_titles( data->mdx );
    }
}

int mdx_next_frame ( t_mdxmini *data )
{
	if (data->self)
	{
		return mdx_parse_mml_ym2151_async(data->self);
	}
	return 0;
}

int mdx_frame_length ( t_mdxmini *data )
{
	if (data->self)
	{
		return mdx_parse_mml_get_tempo(data->self);
	}
	return 0;
}

void mdx_make_buffer( short *buf , int buffer_size )
{
	mdx_parse_mml_ym2151_make_samples(buf , buffer_size);
}

int mdx_calc_sample( t_mdxmini *data, short *buf , int buffer_size )
{
	int s_pos;
	int next,frame;
	
	next = 1;
	s_pos = 0;
	
	do
	{
		if (!data->samples)
		{
			next = mdx_next_frame(data);
			frame = mdx_frame_length(data);
			data->samples = (data->mdx->dsp_speed * frame)/1000000;
		}
		if (data->samples + s_pos >= buffer_size)
		{
			mdx_parse_mml_ym2151_make_samples(buf + (s_pos * data->channels) , buffer_size - s_pos);
			
			data->samples -= (buffer_size - s_pos);
			s_pos = buffer_size;
		}
		else
		{
			mdx_parse_mml_ym2151_make_samples(buf + (s_pos * data->channels) , data->samples);
			s_pos += data->samples;
			data->samples = 0;
		}
		
	}while(s_pos < buffer_size);

	return next;
}

void mdx_get_title( t_mdxmini *data, char *title )
{
	strcpy(title,data->mdx->data_title);
}

int  mdx_get_length( t_mdxmini *data )
{
	return mdx_parse_mml_ym2151_async_get_length(data->self);
}

int  mdx_get_tracks ( t_mdxmini *data )
{
	return data->mdx->tracks;
}

void mdx_get_current_notes ( t_mdxmini *data , int *notes , int len )
{
	int i;
	
	for ( i = 0; i < len; i++ )
	{
		notes[i] = data->mdx->track[i].note;
	}
}

void mdx_stop( t_mdxmini *data )
{
    /* one playing finished */
	
	if (data->self)
		mdx_parse_mml_ym2151_async_finalize(data->self);
    
    mdx_close_pdx( data->pdx );
    mdx_close_mdx( data->mdx );

    self_destroy();
}

int  mdx_get_sample_size ( void )
{
	return pcm8_get_sample_size();
}

int  mdx_get_buffer_size ( void )
{
	return pcm8_get_buffer_size();
}

/* pdx loading */

static unsigned char*
_load_pdx_data(char* name, long* out_length) 
{
  int len;
  FILE *fp;
  unsigned char *buf = NULL;
  
  fp = fopen(name,"rb");
  if (!fp)
	return NULL;

  fseek(fp , 0 ,SEEK_END);
  len = (int)ftell(fp);
  fseek(fp , 0 , SEEK_SET);
  
  
  buf = (unsigned char *)malloc(sizeof(unsigned char) * len);
  if ( !buf ) {
    goto error_end;
  }

  len = (int)fread(buf, 1, len, fp );
  if (len<0) {
    goto error_end;
  }
  fclose(fp);
  
  *out_length  = len;
  
  return buf;

error_end:
  if (fp)
	fclose(fp);
  if (buf)
	free(buf);

  return NULL;
}

static PDX_DATA*
_open_pdx(char* name)
{
  unsigned char* buf = NULL;
  long length = 0;
  PDX_DATA* pdx = NULL;

  buf = _load_pdx_data(name, &length);
  if (!buf) {
    return NULL;
  }
  pdx = mdx_open_pdx(buf, length);
  free(buf);
  return pdx;
}

static PDX_DATA*
_get_pdx(MDX_DATA* mdx, char* mdxpath)
{
  char *a = NULL;
  char buf[PATH_BUF_SIZE];
  PDX_DATA* pdx = NULL;

  mdx->pdx_enable = FLAG_FALSE;
  if ( mdx->haspdx == FLAG_FALSE ) goto no_pdx_file;

   
  /* mdx file path directory */
  
  memset(buf, 0, PATH_BUF_SIZE);
  strncpy( buf, mdxpath, PATH_BUF_SIZE-1 );
  if ( (a=strrchr( buf, '/' )) != NULL ) 
  {
    *(a+1)='\0';
  }
  else
    buf[0] = 0;
  
  strcat( buf, mdx->pdx_name );
  if ( (pdx=_open_pdx( buf )) == NULL ) 
  {
    // specified pdx directory
	strcpy( buf, mdx->pdx_dir );
	int len = (int)strlen( buf );
	
	if (len > 0 && buf [ len - 1 ] != '/' )
			strcat( buf, "/" );
	
	strcat( buf, mdx->pdx_name );
	if ((pdx=_open_pdx( buf )) != NULL )
		goto get_pdx_file;
  }
  else
    goto get_pdx_file;
  

no_pdx_file:
  goto unget_pdx_file;
    
unget_pdx_file:
// tempo could be changed in a pcm track
  mdx->haspdx = FLAG_FALSE;
  mdx->pdx_enable = FLAG_TRUE;
  return NULL;

get_pdx_file:
  mdx->pdx_enable = FLAG_TRUE;
  return pdx;
}

static void* self_mdx2151 = NULL;
static void* self_mdxmml_ym2151 = NULL;
static void* self_pcm8 = NULL;
static void* self_ym2151_c = NULL;

void*
_get_mdx2151(void)
{
  return self_mdx2151;
}

void*
_get_mdxmml_ym2151(void)
{
  return self_mdxmml_ym2151;
}

void*
_get_pcm8(void)
{
  return self_pcm8;
}

void*
_get_ym2151_c(void)
{
  return self_ym2151_c;
}

static int
self_construct(void)
{
  self_mdx2151 = _mdx2151_initialize();
  if (!self_mdx2151) {
    goto error_end;
  }
  self_mdxmml_ym2151 = _mdxmml_ym2151_initialize();
  if (!self_mdxmml_ym2151) {
    goto error_end;
  }
  self_pcm8 = _pcm8_initialize();
  if (!self_pcm8) {
    goto error_end;
  }
#if 0
  self_ym2151_c = _ym2151_c_initialize();
  if (!self_ym2151_c) {
    goto error_end;
  }
#endif

  return FLAG_TRUE;

error_end:
#if 0
  if (self_ym2151_c) {
    _ym2151_c_finalize(self_ym2151_c);
    self_ym2151_c = NULL;
  }
#endif
  if (self_pcm8) {
    _pcm8_finalize(self_pcm8);
    self_pcm8 = NULL;
  }
  if (self_mdxmml_ym2151) {
    _mdxmml_ym2151_finalize(self_mdxmml_ym2151);
    self_mdxmml_ym2151 = NULL;
  }
  if (self_mdx2151) {
    _mdx2151_finalize(self_mdx2151);
    self_mdx2151 = NULL;
  }
  return FLAG_FALSE;
}

static void
self_destroy(void)
{
#if 0
  if (self_ym2151_c) {
    _ym2151_c_finalize(self_ym2151_c);
    self_ym2151_c = NULL;
  }
#endif
  if (self_pcm8) {
    _pcm8_finalize(self_pcm8);
    self_pcm8 = NULL;
  }
  if (self_mdxmml_ym2151) {
    _mdxmml_ym2151_finalize(self_mdxmml_ym2151);
    self_mdxmml_ym2151 = NULL;
  }
  if (self_mdx2151) {
    _mdx2151_finalize(self_mdx2151);
    self_mdx2151 = NULL;
  }
}
