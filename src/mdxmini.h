/**
 * \file mdxmini.h
 * \author BouKiCHi
 * \author Misty De Meo
 * \date 3 January 2016
 */

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

/**
 * \brief Sets the frequency at which raw PCM data is generated.
 *        This is global and affects every song.
 *
 * \param freq a frequency in Hz, for example 44100.
 */
void mdx_set_rate(int freq);
/**
 * \brief Changes the directory in which the provided t_mdxmini context should look for PDX samples.
 *
 * \param data a t_mdxmini struct.
 * \param dir a fully-qualified path on disk in which PDX samples are located.
 */
void mdx_set_dir(t_mdxmini *data, char *dir);
/**
 * \brief Initializes a t_mdxmini struct.
 *        Must be called before any other operations.
 *
 * \param data a t_mdxmini struct.
 * \param filename the full path on disk to an MDX song.
 * \param pcmdir the full path on disk to a directory containing PDX samples to be used with this song. Can be null if the song doesn't use PCM samples.
 * \return 0 on success, -1 if the file does not exist or cannot be recognized as an MDX song.
 */
int  mdx_open(t_mdxmini *data, char *filename, char *pcmdir);
/**
 * \brief Finalizes the structs contained within a t_mdxmini struct.
 *
 * \param data the struct to finalize.
 */
void mdx_close(t_mdxmini *data);

/**
 * \brief Advances a frame in the current song without rendering any content.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \return 0 if no more frames exist in the song, 1 otherwise.
 */
int  mdx_next_frame(t_mdxmini *data);
/**
 * \brief Returns the length of a frame in the current song, in microseconds.
 *
 * \param data a t_mdxmini struct representing an open song.
 */
int  mdx_frame_length(t_mdxmini *data);
/**
 * \brief Renders `buffer_size` bytes of raw PCM data into `buf` and advances the intenal song position.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \param buf a buffer in which songdata will be rendered.
 * \param buffer_size the length, in bytes, of data to render into the buffer.
 */
int  mdx_calc_sample(t_mdxmini *data, short *buf, int buffer_size);
/**
 * \brief Similar to mdx_calc_sample, but does not actually render any content into `buf`.
 */
int  mdx_calc_log(t_mdxmini *data, short *buf, int buffer_size);

/**
 * \brief Copies the title from the song loaded in the passed t_mdxmini struct into the provided string pointer.
 *        The normal encoding of MDX songs is Shift-JIS, but this can contain data in any arbitrary encoding.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \param title the destination string.
 */
void  mdx_get_title(t_mdxmini *data, char *title);
/**
 * \brief Returns the length to the song, in seconds.
 *        This is based on the currently defined number of loops to play, so this value can change.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \return the length to the song, in seconds.
 */
int   mdx_get_length(t_mdxmini *data);
/**
 * \brief Sets the number of loops to play when rendering this song; this is used when calculating duration.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \param loop the number of loops to play; default is 3.
 */
void  mdx_set_max_loop(t_mdxmini *data, int loop);

/**
 * \brief Returns the number of tracks in the song.
 *
 * Every song will contain exactly 8 FM tracks; a result larger than 8 indicates the presence of PCM tracks.
 * For example, this function will return 9 for a song with a single PCM track.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \return the number of tracks in the song.
 */
int  mdx_get_tracks(t_mdxmini *data);
/**
 * \brief Retrieves the notes being played in the current frame.
 *
 * Retrieves the notes being played in the current frame; this is frequently useful for visualization purposes.
 * Writes an array of `int` into the target pointer for up to the requested number of tracks.
 * If the given channel is an FM channel, the integer represents the note being played within 12-note octaves.
 * If the given channel is a PCM channel, the integer just represents the index of the PCM sample being played, which is arbitrary and song-dependent.
 *
 * \param data a t_mdxmini struct representing an open song.
 * \param notes a pointer into which note data will be written.
 * \param len the number of tracks' worth of notes to read; usually the output of mdx_get_tracks().
 */
void mdx_get_current_notes(t_mdxmini *data, int *notes, int len);

#endif
