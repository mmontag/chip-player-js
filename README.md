mdxmini
=======

mdxmini is a C library with a simple API which can play back [MDX](https://en.wikipedia.org/wiki/X68000%27s_MDX) chiptunes from the [X68000 home computer](https://en.wikipedia.org/wiki/X68000). The X68000 was, for its time, an advanced 16-bit computer with excellent multimedia support; its 8-channel [YM2151](https://en.wikipedia.org/wiki/Yamaha_YM2151) sound chip and 1-channel PCM chip gave it excellent music support. A large number of MDX files can be found at the [Fossil of FM-sound era](http://www42.tok2.com/home/mdxoarchive) MDX archive site.

Usage
=====

Detailed API documentation is available in the mdxmini.h header file, but the following sample program shows how to use mdxmini to open a file, extract some metadata, and play back music:

```c
// Initializes the t_mdxmini struct and opens the file from disk.
// The `path_to_samples` argument is the path to the directory containing the song's
// .pdx files, if any; this can be NULL.
t_mdxmini data;
int success = mdx_open(&data, path_to_file, path_to_samples);
// The return value of mdx_open indicates whether or not the file was opened correctly.
if (success == -1) {
    fprintf(stderr, "Failed to open input file: %s\n", path_to_file);
    return 1;
}

// Before playback begins, call mdx_set_rate to set the preferred sampling rate
// for generated audio.
int playback_rate = 48000;
mdx_set_rate(playback_rate);

// Get the song duration, in whole seconds.
int length = mdx_get_length(&data);
printf("Song length: %i seconds\n", length);

// mdx_get_title allows the song's title to be fetched; this is usually
// encoded in Shift-JIS.
char title[MDX_MAX_TITLE_LENGTH];
mdx_get_title(&data, title);
printf("Title: %s\n", title);

// For playback, define a buffer into which we'll render raw PCM data.
int buf_len = 8192;
short buf[buf_len];
// Number of channels, which we'll fetch for display later.
// This is constant for a given song, so just fetch it once.
int number_of_channels = mdx_get_tracks(&data);
printf("Number of channels: %i\n", number_of_channels);
// A buffer into which we'll write information about the notes being
// played in a given frame. 16 is the maximum number of channels supported by MDX
// (8 FM channels, up to 8 PCM channels).
int notes[16];

int position;

// mdx_calc_sample will return 0 when the final frame of the song is reached.
int are_samples_remaining = 1;
// Track the number of buffers played so far, which is useful to calculate
// the current position.
int played_buffers = 0;

// The playback loop!
while (are_samples_remaining == 1)
{
    // Calculate the song position based on the buffer size and frequency.
    position = played_buffers / (((playback_rate * 16 * 2) / 8) / (buf_len * 2));
    printf("Current position: %i / %i\n", position, length);

    are_samples_remaining = mdx_calc_sample(&data, buf, buf_len / 2);
    // Also useful to check the position against the reported duration,
    // as the song may not terminate itself if it can loop infinitely.
    if (position >= length) {
        are_samples_remaining = 0;
    }

    // Do something with the calculated sample here; this is platform-dependent,
    // so this intro will omit it.
    
    // Fill the note buffer with information about the notes in the current frame
    mdx_get_current_notes(&data, notes, number_of_channels);

    // Channels 0 through 7 are FM channels; any MDX file will always have these tracks.
    // The integers for these channels represents a note, divided into standard octaves of 12;
    // for example, 0 would be C in the lowest octave, and 24 would be C two octaves up.
    // These values can be useful for visualizations;
    // for example, the ruby-mdxplay demo program maps these values to piano keys:
    // https://github.com/mistydemeo/ruby-mdxplay/blob/8f54c28cefcfcbdee70eca35ca9a93187385eb8f/bin/mdxplay#L93-L175
    for (int i = 0; i <= 7; i++) {
        printf("Note for FM channel %i is %i\n", i, notes[i]);
    }
    // Channels 8 through 15, if present, are PCM channels.
    // The original X68000's sound chip has one PCM channel;
    // through an expansion card, up to 8 PCM channels may be present.
    // The integers for these channels indicate the index of the PCM sample in use;
    // this is less informative than the note value, but a visualization might
    // still be able to use this.
    for (int i = 8; i < number_of_channels; i++) {
        printf("Sample for PCM channel %i is %i\n", i, notes[i]);
    }

    // Increment the count of played buffers.
    played_buffers++;
}

// When playback is over, finalize the t_mdxmini struct.
mdx_close(&data);
```

Programs using mdxmini
======================

* [MDXPLAYER for Android](https://github.com/mistydemeo/mdxplayer), by BouKiCHi
* [ruby-mdxplayer](https://github.com/mistydemeo/ruby-mdxplay)
* [Paula](https://github.com/mistydemeo/paula)
* [Modizer](http://yoyofr.blogspot.ca/p/modizer.html) by [yoyofr](http://yoyofr.blogspot.ca)

Credits
=======

* milk.K, K.MAEKAWA, Missy.M - authors of the original X68000 MXDRV
* [Daisuke Nagano](http://web.archive.org/web/20101015100349/http://homepage3.nifty.com/StudioBreeze/) - author of the Unix [mdxplay](http://web.archive.org/web/20130217181839/http://homepage3.nifty.com/StudioBreeze/software/mdxplay-e.html)
* [BouKiCHi](http://clogging.blog57.fc2.com) - author of the mdxmini library, and of the Android mdxplayer
* [Misty De Meo](http://www.mistys-internet.website) - bugfixes and improvements to mdxmini, current maintainer
