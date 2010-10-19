package org.helllabs.android.xmp;

import android.widget.LinearLayout;
import android.widget.TextView;

abstract public class Meter {
	
	int numChannels = 0;
	TextView infoMeter[] = new TextView[32];
	int[] oldVol = new int[32];
	
	public Meter(LinearLayout layout, int chn) {
		int i;
		
		numChannels = chn;
		
		for (i = 0; i < 32; i++) {
			infoMeter[i] = new TextView(layout.getContext());
		}
		
		layout.removeAllViews();
		for (i = 0; i < chn; i++) {
			if (i >= 32)
				break;
			layout.addView(infoMeter[i], i);
		}
		
		reset();
   	}

	public void reset() {
		for (int i = 0; i < 32; i++) {
			oldVol[i] = -1;
		}
	}
	
	abstract void setVolumes(int[] vol);
}
