package org.helllabs.android.xmp;

import android.graphics.Color;
import android.util.TypedValue;
import android.widget.LinearLayout;

public class BarMeter extends Meter {
	
	String[] bar = { "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };
	
	public BarMeter(LinearLayout layout, int chn) {
		super(layout, chn);

		float scale = 0.6f;
		
		if (chn <= 4)
			scale = 1.6f;
		else if (chn <= 10)
			scale = 0.8f;
		else if (chn > 24)
			scale = 0.4f;
		
		for (int i = 0; i < MAX_METERS; i++) {
			infoMeter[i].setText(bar[0]);
			infoMeter[i].setTextColor(Color.rgb(64, 112, 192));
			infoMeter[i].setPadding(1, 0, 1, 0);
			infoMeter[i].setTextSize(TypedValue.COMPLEX_UNIT_DIP, 32);
			infoMeter[i].setTextScaleX(scale);
		}
		
		type = 2;
	}

	@Override
	public void setVolumes(int[] vol) {
		int max = numChannels < MAX_METERS ? numChannels : MAX_METERS;
		for (int i = 0; i < max; i++) {
			int v = vol[i] / 8;
			if (v > 7)
				v = 7;
			if (v != oldVol[i]) {
				infoMeter[i].setText(bar[v]);
				oldVol[i] = v;
			}
		}
	}
}
