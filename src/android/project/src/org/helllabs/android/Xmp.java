package org.helllabs.android;

public class Xmp {
	private native int init();
	private native int deinit();
	private native int load(String name);
	private native int release();
	private native int play();
	private native int stop();
	private native int play_frame();	
	private native int softmixer();
	private native short[] get_buffer(int size);
}
