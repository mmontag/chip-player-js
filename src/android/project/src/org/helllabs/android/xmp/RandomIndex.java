package org.helllabs.android.xmp;

import java.util.Date;
import java.util.Random;


public class RandomIndex {
	private int[] idx;
	private Random random;
	
	public RandomIndex(int n) {
		idx = new int[n];
		
		random = new Random();
		Date date = new Date();
		random.setSeed(date.getTime());
		
		for (int i = 0; i < n; i++) {
			idx[i] = i;
		}
		
		randomize(0, n);
	}

	public void randomize(int start, int length) {
		int end = start + length;
		for (int i = start; i < end; i++) {				
			int r = start + random.nextInt(length);
			int temp = idx[i];
			idx[i] = idx[r];
			idx[r] = temp;
		}
	}
	
	public void randomize() {
		randomize(0, idx.length);
	}
	
	public void extend(int amount) {
		int length = idx.length;
		int[] newIdx = new int[length + amount];
		System.arraycopy(idx, 0, newIdx, 0, length);
		for (int i = length; i < length + amount; i++)
			newIdx[i] = i;
		idx = newIdx;
		randomize(length, amount);
	}
	
	public int getIndex(int n) {
		return idx[n];
	}
}
