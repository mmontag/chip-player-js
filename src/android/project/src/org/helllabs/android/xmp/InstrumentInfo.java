package org.helllabs.android.xmp;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.widget.TextView;

public class InstrumentInfo extends TextView {
	int curVol;
	
	public InstrumentInfo(Context context) {
		super(context);
		curVol = -1;
	}
	
	public void setText(int i, CharSequence s) {
		setText(String.format("%02x: %s", i, s));
		setTypeface(Typeface.MONOSPACE);
		setSingleLine(true);
		setIncludeFontPadding(false);
		setTextColor(Color.rgb(0x80, 0x80, 0x80));
	}
	
	public void showVolume(int vol) {
		vol &= 0xf8;
		if (vol != curVol) {
			int val = 0x80 + vol;
			setTextColor(Color.rgb(val, val, val));
			curVol = vol & 0xf8;
		}
	}
}
