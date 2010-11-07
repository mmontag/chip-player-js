package org.helllabs.android.xmp;

import android.graphics.Color;
import android.widget.LinearLayout;

public class LedMeter extends Meter {	
	
	public LedMeter(LinearLayout layout, int chn) {
		super(layout, chn);

		for (int i = 0; i < MAX_METERS; i++) {
			infoMeter[i].setText("●");
			infoMeter[i].setTextColor(Color.rgb(0, 50, 0));
		}
		
		type = 1;
	}

	@Override
	public void setVolumes(int[] vol) {
		int max = numChannels < MAX_METERS ? numChannels : MAX_METERS;
		for (int i = 0; i < max; i++) {
			if (vol[i] != oldVol[i]) {
				infoMeter[i].setTextColor(Color.rgb(0, 50 + vol[i] * 3, 0));
				oldVol[i] = vol[i];
			}
		}
	}
}