package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.Arrays;

public class QueueManager {
	private ArrayList<String> array;
	private RandomIndex ridx;
	private int index;
	private boolean shuffleMode;
    
    public QueueManager(String[] files, boolean shuffle) {
    	index = 0;
    	array = new ArrayList<String>(Arrays.asList(files));
    	ridx = new RandomIndex(files.length);
    	shuffleMode = shuffle;
    }
    
    public void add(String[] files) {
    	ridx.extend(files.length);
		for (String s : files)
			array.add(s);
    }
    
    public int size() {
    	return array.size();
    }
    
    public void next() {
    	index++;
    	if (index >= array.size())
    		index = 0;
    }
    
    public void previous() {
    	index--;
    	if (index < 0)
    		index = array.size() - 1;
    }
    
    public String getFilename() {
    	int idx = shuffleMode ? ridx.getIndex(index) : index;
    	return array.get(idx);
    }
}
