package org.helllabs.android.xmp;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.ToggleButton;

class ChannelInfo extends LinearLayout {
	ToggleButton button;
	TextView led;
	TextView note;
	TextView instrument;
	boolean state;
	
	public ChannelInfo(Context context) {
		super(context);

		setOrientation(LinearLayout.HORIZONTAL);
		
		button = new ToggleButton(context);
		addView(button);
		
		led = new TextView(context);
		led.setText("●");
		addView(led);
		
		note = new TextView(context);
		note.setTypeface(Typeface.MONOSPACE);
		note.setText("---");
		addView(note);
		
		instrument = new TextView(context);
		addView(instrument);
		
		state = true;
	}
	
	public void setText(CharSequence s) {
		button.setText(s);
	}
	
	public void toggle(boolean b) {
		
	}
	
	public void setLed(int n) {
		led.setTextColor(Color.rgb(0, 50 + n * 3, 0));
	}
	
	public void setNote(int n) {
		note.setText("C#4");
	}
	
	public void setInstrument(int n) {
		
	}
}

