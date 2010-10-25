package org.helllabs.android.xmp;

import android.content.Context;
import android.widget.Toast;

public class Message {
	
	public static void error(String s) {
		
	}
	
	public static void toast(Context context, String s) {
		Toast.makeText(context, s, Toast.LENGTH_SHORT).show();		
	}

}
