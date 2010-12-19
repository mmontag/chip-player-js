package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.Arrays;

public class QueueManager {
	private ArrayList<String> array;
	private RandomIndex ridx;
	private int index;
	private boolean shuffleMode;
	private boolean loopListMode;
    
    public QueueManager(String[] files, boolean shuffle, boolean loop) {
    	index = 0;
    	array = new ArrayList<String>(Arrays.asList(files));
    	ridx = new RandomIndex(files.length);
    	shuffleMode = shuffle;
    	loopListMode = loop;
    }
    
    public void add(String[] files) {
    	if (files.length > 0) {
    		ridx.extend(files.length, index + 1);
    		for (String s : files)
    			array.add(s);
    	}
    }
    
    public int size() {
    	return array.size();
    }
    
    public boolean next() {
    	index++;
    	if (index >= array.size()) {
    		if (loopListMode) {
    			ridx.randomize();
    			index = 0;
    		} else {
    			return false;
    		}
    	}
    	return true;
    }
    
    public int index() {
    	return index;
    }
    
    public void previous() {
    	index -= 2;
    	if (index < -1) {
    		if (loopListMode)
    			index += array.size();
    		else
    			index = -1;
    	}
    }
    
    public void restart() {
    	index = -1;
    }
    
    public String getFilename() {
    	int idx = shuffleMode ? ridx.getIndex(index) : index;
    	return array.get(idx);
    }
}
