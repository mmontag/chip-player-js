package com.helloandroid.android.musicdroid3;

interface MDSInterface {
	void clearPlaylist();
	void addSongPlaylist( in String song ); 
	void playFile( in int position );

	void pause();
	void stop();
	void skipForward();
	void skipBack(); 
} 