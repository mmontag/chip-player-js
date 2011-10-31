package org.helllabs.android.xmp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/*
 * From "Handling Screen OFF and Screen ON Intents" by jwei512
 * http://thinkandroid.wordpress.com/2010/01/24/handling-screen-off-and-screen-on-intents/
 */

public class ScreenReceiver extends BroadcastReceiver {
	
	// THANKS JASON
	public static boolean wasScreenOn = true;

	@Override
	public void onReceive(Context context, Intent intent) {
		if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF)) {
			// DO WHATEVER YOU NEED TO DO HERE
			wasScreenOn = false;
		} else if (intent.getAction().equals(Intent.ACTION_SCREEN_ON)) {
			// AND DO WHATEVER YOU NEED TO DO HERE
			wasScreenOn = true;
		}
	}
}

