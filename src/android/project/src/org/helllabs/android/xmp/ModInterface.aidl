package org.helllabs.android.xmp;

import org.helllabs.android.xmp.PlayerCallback;

interface ModInterface {
	void play(in String[] files, boolean shuffle, boolean loopList);
	void add(in String[] files);
	void stop();
	void pause();
	int time();
	void seek(in int seconds);
	int getPlayTempo();
	int getPlayBpm();
	int getPlayPos();
	int getPlayPat();
	void getVolumes(out int[] volumes);
	void nextSong();
	void prevSong(); 
	boolean isPaused();
	boolean toggleLoop();
	String getFileName();
	String[] getInstruments();
	
	void registerCallback(PlayerCallback cb);
	void unregisterCallback(PlayerCallback cb);
}