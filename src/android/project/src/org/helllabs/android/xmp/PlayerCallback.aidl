package org.helllabs.android.xmp;

interface PlayerCallback {
	void newModCallback(String name, in String[] instruments);
	void endPlayCallback();
}