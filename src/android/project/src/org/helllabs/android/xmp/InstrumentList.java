package org.helllabs.android.xmp;

import android.content.Context;
import android.widget.LinearLayout;

public class InstrumentList extends LinearLayout {
	Context context;
	InstrumentInfo[] instrumentInfo;
	int num;
	
	public InstrumentList(Context context) {
		super(context);
		this.context = context;
		setOrientation(VERTICAL);
	}
	
	public void setInstruments(String[] list) {
		num = list.length;
	
		instrumentInfo = new InstrumentInfo[num];
		
		removeAllViews();
		for (int i = 0; i < num; i++) {
       		instrumentInfo[i] = new InstrumentInfo(context);
       		instrumentInfo[i].setText(i, list[i]);
       		addView(instrumentInfo[i]);			
		}		
	}
	
	/*
	 * Check if instrument is playing in a channel, and update instrument
	 * color accordingly.
	 */
	public void setVolumes(int[] volumes, int[] instruments) {
		int nch = volumes.length;
		boolean flag;
		
		for (int i = 0; i < num; i++) {
			flag = false;
			
			for (int j = 0; j < nch; j++) {
				if (instruments[j] == i) {
					instrumentInfo[i].showVolume(volumes[j]);
					flag = true;
				}
			}
			
			if (!flag) {
				instrumentInfo[i].showVolume(0);
			}
		}
	}
}
