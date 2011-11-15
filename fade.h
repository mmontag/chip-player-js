

int   fade_count = 0;
float fade_step = 0;
float fade_volume = 1;

static void fade_init()
{
	fade_count = 0;
	fade_volume = 1;
	fade_step = 0;
}

static void fade_start(int rate,int sec)
{
	fade_count = rate * sec;
	fade_step = ((float)1)/fade_count;
	fade_volume = 1;
	
}

static void fade_stereo(short *data,int len)
{
	// stereo
	int i = 0;
	for ( i = 0; i < len * 2; i += 2 )
	{
		data[i] = ((float)data[i]) * fade_volume;
		data[i+1] = ((float)data[i+1]) * fade_volume;
		if (fade_count > 0)
		{
			fade_count--;
			if (fade_volume > 0)
				fade_volume -= fade_step;
			else
				fade_volume = 0;
			
			// printf("fv=%f\n",fade_volume);
			
		} else
			fade_volume = 0;
	}
}

static int is_fade_run(void)
{
	if ( fade_count > 0 || fade_volume == 0 )
		return 1;
	
	return 0;
}

static int is_fade_end(void)
{
	if ( fade_volume > 0 || fade_count > 0 )
		return 0;

	return 1;
}

