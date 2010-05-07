package org.helllabs.android.xmp;

public class Xmp {
	public native int init();
	public native int deinit();
	public native int testModule(String name);
	public native int loadModule(String name);
	public native int releaseModule();
	public native int startPlayer();
	public native int endPlayer();
	public native int playFrame();	
	public native int softmixer();
	public native short[] getBuffer(int size);
	public native int nextOrd();
	public native int prevOrd();
	public native int setOrd(int n);
	public native int stopModule();
	public native int restartModule();
	public native int stopTimer();
	public native int restartTimer();
	public native int incGvol();
	public native int decGvol();
	public native int seek(long time);
	
	static {
		System.loadLibrary("xmp");
	}
}
