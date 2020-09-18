/*
* This file adapts "LazyUSF2" to the interface expected by my generic JavaScript player..
*
* Copyright (C) 2018 Juergen Wothke
*
* LICENSE
* 
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

// problem: it seems _lib files here may again recursively refer to other _lib files
// e.g. Ultra64 Sound Format/Yasunori Mitsuda/Mario Party/mp1_1.usflib -> mp1rom.usflib

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>     /* malloc, free, rand */

#include <exception>
#include <iostream>
#include <fstream>


#ifdef EMSCRIPTEN
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define BUF_SIZE	1024
#define TEXT_MAX	255
#define NUM_MAX	15

// see Sound::Sample::CHANNELS
#define CHANNELS 2				
#define BYTES_PER_SAMPLE 2
#define SAMPLE_BUF_SIZE	1024

#define t_int16   signed short
t_int16 sample_buffer[SAMPLE_BUF_SIZE * CHANNELS];
int samples_available= 0;

const char* info_texts[7];

char title_str[TEXT_MAX];
char artist_str[TEXT_MAX];
char game_str[TEXT_MAX];
char year_str[TEXT_MAX];
char genre_str[TEXT_MAX];
char copyright_str[TEXT_MAX];
char psfby_str[TEXT_MAX];


// interface to n64plug.cpp
extern	void n64_setup (void);
extern	void n64_boost_volume(unsigned char b);
extern	int32_t n64_get_samples_to_play ();
extern	int32_t n64_get_samples_played ();
extern	int32_t n64_get_sample_rate ();
extern	int n64_load_file(const char *uri, int16_t *output_buffer, uint16_t outSize);
extern	int n64_read(int16_t *output_buffer, uint16_t outSize);
extern	int n64_seek_sample (int sampleTime);

void n64_meta_set(const char * tag, const char * value) {
	// propagate selected meta info for use in GUI
	if (!strcasecmp(tag, "title")) {
		snprintf(title_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "artist")) {
		snprintf(artist_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "album")) {
		snprintf(game_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "date")) {
		snprintf(year_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "genre")) {
		snprintf(genre_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "copyright")) {
		snprintf(copyright_str, TEXT_MAX, "%s", value);
		
	} else if (!strcasecmp(tag, "usfby")) {
		snprintf(psfby_str, TEXT_MAX, "%s", value);		
	} 
}

struct StaticBlock {
    StaticBlock(){
		info_texts[0]= title_str;
		info_texts[1]= artist_str;
		info_texts[2]= game_str;
		info_texts[3]= year_str;
		info_texts[4]= genre_str;
		info_texts[5]= copyright_str;
		info_texts[6]= psfby_str;
    }
};
	
void meta_clear() {
	snprintf(title_str, TEXT_MAX, "");
	snprintf(artist_str, TEXT_MAX, "");
	snprintf(game_str, TEXT_MAX, "");
	snprintf(year_str, TEXT_MAX, "");
	snprintf(genre_str, TEXT_MAX, "");
	snprintf(copyright_str, TEXT_MAX, "");
	snprintf(psfby_str, TEXT_MAX, "");
}


static StaticBlock g_emscripen_info;

extern "C" void emu_teardown (void)  __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_teardown (void) {
}

extern "C" int emu_setup(char *unused) __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_setup(char *unused)
{
	n64_setup();	// basic init
	
	return 0;
}

extern "C" int emu_init(char *basedir, char *songmodule) __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_init(char *basedir, char *songmodule)
{
	n64_setup();	// basic init

	emu_teardown();

	meta_clear();
	
	std::string file= std::string(basedir)+"/"+std::string(songmodule);
	
	
	if (n64_load_file(file.c_str(), sample_buffer, SAMPLE_BUF_SIZE) == 0) {
	} else {
		return -1;
	}
	return 0;
}

extern "C" int emu_get_sample_rate() __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_get_sample_rate()
{
	return n64_get_sample_rate();
}

extern "C" int emu_set_subsong(int subsong, unsigned char boost) __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_set_subsong(int subsong, unsigned char boost) {
// TODO: are there any subsongs
	n64_boost_volume(boost);
	return 0;
}

extern "C" const char** emu_get_track_info() __attribute__((noinline));
extern "C" const char** EMSCRIPTEN_KEEPALIVE emu_get_track_info() {
	return info_texts;
}

extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) __attribute__((noinline));
extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) {
	return (char*)sample_buffer;
}

extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) __attribute__((noinline));
extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) {
	return samples_available;
}

extern "C" int emu_compute_audio_samples() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_compute_audio_samples() {
	uint16_t size = SAMPLE_BUF_SIZE;

	int ret=  n64_read((short*)sample_buffer, size);	// returns number of bytes

	if (ret < 0) {
		samples_available= 0;
		return 1;
	} else {
		samples_available= ret; // available time (measured in samples)
		if (ret) {
			return 0;
		} else {
			return 1;
		}		
	}
}

extern "C" int emu_get_current_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_current_position() {
	return n64_get_samples_played();
}

extern "C" void emu_seek_position(int pos) __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_seek_position(int sampleTime) {
	// FIXME: before seeking the player should pause playback and/or clear the WebAudio buffer	
	n64_seek_sample(sampleTime);
}

extern "C" int emu_get_max_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_max_position() {
	return n64_get_samples_to_play();
}

