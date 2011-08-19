package org.helllabs.android.xmp;

import android.content.Context;
import android.graphics.Typeface;
import android.widget.TextView;

public class InstrumentInfo extends TextView {
	
	public InstrumentInfo(Context context) {
		super(context);
	}
	
	public void setText(int i, CharSequence s) {
		setText(String.format("%02x: %s", i, s));
		setTypeface(Typeface.MONOSPACE);
		setSingleLine(true);
		setIncludeFontPadding(false);
	}

}
