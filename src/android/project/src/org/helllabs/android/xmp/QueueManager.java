package org.helllabs.android.xmp;

import java.util.ArrayList;

public class QueueManager {
	ArrayList<QueueItem> array;
	
    private class QueueItem {
    	boolean played;
    	String filename;
    	
    	public QueueItem(String s) {
    		filename = s;
    		played = false;
    	}
    }
    
    public QueueManager(String[] files) {
    	array = new ArrayList<QueueItem>();
		for (String s : files)
			array.add(new QueueItem(s));
    }
    
    public void add(String[] files) {
		for (String s : files)
			array.add(new QueueItem(s));
    }
    
    public int size() {
    	return array.size();
    }
    
    public String getFilename(int index) {
    	return array.get(index).filename;
    }
    
    public void setPlayed(int index, boolean b) {
    	array.get(index).played = b;
    }
}
