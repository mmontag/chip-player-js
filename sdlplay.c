#include <stdio.h>
#include <SDL.h>


#include "mdxmini.h"

int audio_on = 0;

#define PCM_BLOCK 512
#define PCM_BYTE_PER_SAMPLE 2
#define PCM_CH  2


#define BUF_BLKSIZE PCM_BLOCK


#define BUF_BLKNUM 16

#define BUF_LEN ( BUF_BLKSIZE * PCM_CH * BUF_BLKNUM )

int buf_wpos = 0;
int buf_ppos = 0;

short buffer[ BUF_LEN ];

void audio_callback(void *param,Uint8 *data,int len)
{
	int i,j;
	
	short *pcm = (short *)data;

	if (!audio_on)
	{
		memset(data,0,len);
		return;
	}
	
	for( i = 0; i < len/2; i++ )
	{
		pcm[i] = buffer[buf_ppos++];
		if (buf_ppos >= BUF_LEN)
				buf_ppos = 0;
	}
}


#include "fade.h"


int init_audio( int freq )
{
	SDL_AudioSpec af;

	if (SDL_Init(SDL_INIT_AUDIO))
	{
		printf("Failed to Initialize!!\n");
		return 1;
	}
	
	af.freq = freq;
	af.format = AUDIO_S16;
	af.channels = 2;
	af.samples = 512;
	af.callback = audio_callback;
	af.userdata = NULL;

	if (SDL_OpenAudio(&af,NULL) < 0)
	{
		printf("Audio Error!!\n");
		return 1;
	}
	
	memset(buffer,0,sizeof(buffer));
	
	SDL_PauseAudio(0);
	return 0;
}

void free_audio(void)
{
	SDL_CloseAudio();
}

void player_screen( t_mdxmini *data )
{
	int i,n;
	int notes[32];
	
	n = mdx_get_tracks ( data );
	
	mdx_get_current_notes( data , notes , n );
	
	for ( i = 0; i < 8; i ++ )
	{
		printf("%02x " , ( notes[i] & 0xff ) );
	}
	printf(" ");
		
}

void player_loop( t_mdxmini *data , int freq )
{

	int i,j,len;
	
	int total;
	int sec,old_sec;
	int sec_sample;
	int next;

	len = mdx_get_length( data );
	
//	len = 5;

	mdx_set_max_loop( data , 0 );
	fade_init();
	
	old_sec = total = sec = sec_sample = 0;
	
	next = 1;
	
	do
	{
		// waiting for next block
		while(buf_wpos >= buf_ppos &&
			  buf_wpos <  buf_ppos + ( PCM_BLOCK * PCM_CH ) )
		{
			SDL_Delay(10);
		}
		
		next = mdx_calc_sample( data , buffer + buf_wpos, PCM_BLOCK );
		if (is_fade_run())
			fade_stereo ( buffer + buf_wpos , PCM_BLOCK );
		
		buf_wpos += PCM_BLOCK * PCM_CH;
		if (buf_wpos >= BUF_LEN)
				buf_wpos = 0;
		
		total += PCM_BLOCK;
		sec_sample += PCM_BLOCK;
		while ( sec_sample >= freq )
		{
			sec_sample -= freq;
			sec++;
		}
		if (sec != old_sec)
		{
			if (sec >= (len-3))
			{ 
				if (!is_fade_run())
					fade_start( freq , 1 );
			}
			old_sec = sec;
		}
		
		player_screen ( data );			
		printf("Time : %02d:%02d / %02d:%02d\r\r",
			sec / 60 , sec % 60 , len / 60,len % 60);
		fflush(stdout);				


	}while(sec < len);
	
	SDL_PauseAudio(1);

}



int main ( int argc, char *argv[] )
{
	FILE *fp;
	
	t_mdxmini mini;
	
	int rate = 11025;
	
	
	printf("MDXPLAY on SDL Version 111022\n");
	
	if (argc < 2)
	{
		printf("Usage mdxplay [file]\n");
		return 1;
	}
	
	if (init_audio( rate ))
	{
		return 0;
	}
	
	audio_on = 1;
	
	mdx_set_rate ( rate );
		
	if (mdx_open(&mini,argv[1]))
	{
		printf("File open error\n");
		return 0;
	}
	
	player_loop( &mini , rate );
	
	
	mdx_stop( &mini );

	free_audio();
	

	return 0;
}