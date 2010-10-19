package org.helllabs.android.xmp;

import android.graphics.Color;
import android.widget.LinearLayout;

public class BarMeter extends Meter {
	
	String[] bar = { "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };
	
	public BarMeter(LinearLayout layout, int chn) {
		super(layout, chn);

		for (int i = 0; i < MAX_METERS; i++) {
			infoMeter[i].setText(bar[0]);
			infoMeter[i].setTextColor(Color.rgb(64, 112, 192));
			infoMeter[i].setPadding(1, 0, 1, 0);
		}
		
		type = 2;
	}

	@Override
	public void setVolumes(int[] vol) {
		for (int i = 0; i < numChannels; i++) {
			if (vol[i] != oldVol[i]) {
				int v = vol[i] / 8;
				if (v > 7)
					v = 7;
				infoMeter[i].setText(bar[v]);
				oldVol[i] = vol[i];
			}
		}
	}
}
