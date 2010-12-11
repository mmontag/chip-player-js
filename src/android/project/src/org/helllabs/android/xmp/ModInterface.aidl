package org.helllabs.android.xmp;

import org.helllabs.android.xmp.PlayerCallback;

interface ModInterface {
	void play(in String[] files);
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
	boolean toggleLoop();
	
	void registerCallback(PlayerCallback cb);
	void unregisterCallback(PlayerCallback cb);
} 