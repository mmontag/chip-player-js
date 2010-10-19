package org.helllabs.android.xmp;

import android.graphics.Color;
import android.widget.LinearLayout;
import android.widget.TextView;

public class LedMeter extends Meter {	
	
	public LedMeter(LinearLayout layout, int chn) {
		super(layout, chn);

		for (int i = 0; i < 32; i++) {
			infoMeter[i].setText("●");
			infoMeter[i].setTextColor(Color.rgb(0, 50, 0));
		}
		
		type = 1;
	}

	@Override
	public void setVolumes(int[] vol) {
		for (int i = 0; i < numChannels; i++) {
			if (vol[i] != oldVol[i]) {
				infoMeter[i].setTextColor(Color.rgb(0, 50 + vol[i] * 3, 0));
				oldVol[i] = vol[i];
			}
		}
	}
}