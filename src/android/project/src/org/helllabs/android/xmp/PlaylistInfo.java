package org.helllabs.android.xmp;

public class PlaylistInfo {
	String name;
	String comment;
	String filename;
	
	public PlaylistInfo(String _name, String _comment) {
		name = _name;
		comment = _comment;
	}
	
	public PlaylistInfo(String _name, String _comment, String _filename) {
		name = _name;
		comment = _comment;
		filename = _filename;
	}
}
