package org.helllabs.android.xmp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.view.KeyEvent;

public class RemoteControlReceiver extends BroadcastReceiver {

	public static int keyCode = -1;
	
	@Override
	public void onReceive(Context context, Intent intent) {
		String action = intent.getAction();
		Log.d("Xmp RemoteControlReceiver", "Action " + action);
		if (action.equals(Intent.ACTION_MEDIA_BUTTON)) {
			int code;
			KeyEvent event = (KeyEvent)intent.getExtras().get(Intent.EXTRA_KEY_EVENT);
			
			if (event.getAction() != KeyEvent.ACTION_DOWN)
				return;
			
			switch (code = event.getKeyCode()) {
			case KeyEvent.KEYCODE_MEDIA_NEXT:
			case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
			case KeyEvent.KEYCODE_MEDIA_STOP:
			//case KeyEvent.KEYCODE_MEDIA_PAUSE:
			//case KeyEvent.KEYCODE_MEDIA_PLAY:
			case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
				Log.d("Xmp RemoteControlReceiver", "Key code " + code);
				keyCode = code;
				break;
			default:
				Log.d("Xmp RemoteControlReceiver", "Unhandled key code " + code);
			}
		}
	}
}