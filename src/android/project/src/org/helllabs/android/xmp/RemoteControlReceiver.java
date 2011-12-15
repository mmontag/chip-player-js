package org.helllabs.android.xmp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.view.KeyEvent;

public class RemoteControlReceiver extends BroadcastReceiver {

	public static int keyCode = -1;
	
	@Override
	public void onReceive(Context context, Intent intent) {
		if (Intent.ACTION_MEDIA_BUTTON.equals(intent.getAction())) {
			int code;
			KeyEvent event = (KeyEvent)intent.getExtras().get(Intent.EXTRA_KEY_EVENT);
			switch (code = event.getKeyCode()) {
			case KeyEvent.KEYCODE_MEDIA_NEXT:
			case KeyEvent.KEYCODE_MEDIA_REWIND:
			case KeyEvent.KEYCODE_MEDIA_STOP:
			case KeyEvent.KEYCODE_MEDIA_PAUSE:
			case KeyEvent.KEYCODE_MEDIA_PLAY:
			case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
				keyCode = code;
				break;
			}
		}
	}
}