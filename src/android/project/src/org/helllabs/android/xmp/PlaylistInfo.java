package org.helllabs.android.xmp;

public class PlaylistInfo implements Comparable<PlaylistInfo> {
	String name;
	String comment;
	String filename;
	int imageRes;
	
	public PlaylistInfo(String _name, String _comment, int _imageRes) {
		name = _name;
		comment = _comment;
		imageRes = _imageRes;
	}
	
	public PlaylistInfo(String _name, String _comment, String _filename, int _imageRes) {
		name = _name;
		comment = _comment;
		filename = _filename;
		imageRes = _imageRes;
	}
	
	// Comparable
	
	public int compareTo(PlaylistInfo p) {
		return this.name.compareTo(p.name);
	}

}
