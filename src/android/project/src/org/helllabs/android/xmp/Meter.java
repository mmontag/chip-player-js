package org.helllabs.android.xmp;

import android.widget.LinearLayout;
import android.widget.TextView;

abstract public class Meter {
	final int MAX_METERS = 32;
	
	int numChannels = 0;
	int type;
	TextView infoMeter[] = new TextView[MAX_METERS];
	int[] oldVol = new int[MAX_METERS];
	
	public Meter(LinearLayout layout, int chn) {
		int i;
		
		numChannels = chn;
		layout.setVisibility(LinearLayout.VISIBLE);
		
		for (i = 0; i < MAX_METERS; i++) {
			infoMeter[i] = new TextView(layout.getContext());
		}
		
		layout.removeAllViews();
		for (i = 0; i < chn; i++) {
			if (i >= MAX_METERS)
				break;
			layout.addView(infoMeter[i], i);
		}
		
		reset();
   	}

	public int getChannels() {
		return numChannels;
	}
	
	public int getType() {
		return type;
	}
	
	public void reset() {
		for (int i = 0; i < MAX_METERS; i++) {
			oldVol[i] = -1;
		}
	}
	
	abstract void setVolumes(int[] vol);
}
