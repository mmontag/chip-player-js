package org.helllabs.android;

public class Xmp {
	private native int init();
	private native int deinit();
	private native int load(String name);
	private native int release();
	private native int play();
	private native int stop();
	private native int frame();	
}
