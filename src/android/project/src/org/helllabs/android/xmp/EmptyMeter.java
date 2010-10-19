package org.helllabs.android.xmp;

import android.widget.LinearLayout;

public class EmptyMeter extends Meter {
	
	public EmptyMeter(LinearLayout layout, int chn) {
		super(layout, chn);
		layout.setVisibility(LinearLayout.INVISIBLE);
		type = 0;
	}

	@Override
	public void setVolumes(int[] vol) {
	}
}
