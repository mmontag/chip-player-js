package org.helllabs.android.xmp;

interface ModInterface {
	void play(in String file);
	void stop();
	void pause();
	int time();
	void seek(in int seconds);
	int getPlayTempo();
	int getPlayBpm();
	int getPlayPos();
	int getPlayPat();
	String[] getInstruments();
	void getVolumes(out int[] volumes);
	void nextSong();
	void prevSong(); 
	boolean toggleLoop();
} 