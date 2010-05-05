package org.helllabs.android.xmp;

public class Xmp {
	public native int init();
	public native int deinit();
	public native int load(String name);
	public native int release();
	public native int play();
	public native int stop();
	public native int playFrame();	
	public native int softmixer();
	public native short[] getBuffer(int size);
}
