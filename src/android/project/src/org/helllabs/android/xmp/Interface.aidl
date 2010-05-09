package org.helllabs.android.xmp;

interface Interface {
	void clearPlaylist();
	void addSongPlaylist(in String song);
	void playFile(in int position);

	void pause();
	void stop();
	void skipForward();
	void skipBack();
	
	int time();
	void seekPosition(in int seconds);
}

