package org.helllabs.android.xmp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.Toast;

public class Message {
	
	public static void fatalError(Context context, String message, final Activity a) {
		AlertDialog alertDialog = new AlertDialog.Builder(context).create();
		
		alertDialog.setTitle("Error");
		alertDialog.setMessage(message);
		alertDialog.setButton("Exit", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				a.finish();
			}
		});
		alertDialog.show();		
	}
	
	public static void error(Context context, String message) {
		AlertDialog alertDialog = new AlertDialog.Builder(context).create();
		
		alertDialog.setTitle("Error");
		alertDialog.setMessage(message);
		alertDialog.setButton("Dismiss", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				//
			}
		});
		alertDialog.show();		
	}
		
	
	public static void toast(Context context, String s) {
		Toast.makeText(context, s, Toast.LENGTH_SHORT).show();		
	}

}
