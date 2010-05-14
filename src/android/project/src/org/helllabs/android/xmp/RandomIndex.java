package org.helllabs.android.xmp;

import java.util.Date;
import java.util.Random;

public class RandomIndex {
	private int[] idx;
	
	public RandomIndex(int n) {
		idx = new int[n];
		for (int i = 0; i < n; i++) {
			idx[i] = i;
		}
		
		randomize();
	}

	public void randomize() {
		Random random = new Random();
		Date date = new Date();
		random.setSeed(date.getTime());
		for (int i = 0; i < idx.length; i++) {				
			int r = random.nextInt(idx.length);
			int temp = idx[i];
			idx[i] = idx[r];
			idx[r] = temp;
		}
	}
	
	public int getIndex(int n) {
		return idx[n];
	}
}
