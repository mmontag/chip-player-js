package org.helllabs.android.xmp;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

public class ChangeLog {
	private SharedPreferences prefs;
	private int versionCode, lastViewed;
	private Context context;
	
	public ChangeLog(Context c) {
		context = c;
	}
	
	public int show() {
	    try {
	        PackageInfo packageInfo = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
	        versionCode = packageInfo.versionCode; 

	        prefs = PreferenceManager.getDefaultSharedPreferences(context);
	        lastViewed = prefs.getInt(Settings.PREF_CHANGELOG_VERSION, 0);

	        if (lastViewed < versionCode) {
	            Editor editor = prefs.edit();
	            editor.putInt(Settings.PREF_CHANGELOG_VERSION, versionCode);
	            editor.commit();
	            showLog();
	            return 0;
	        } else {
	        	return -1;
	        }
	    } catch (NameNotFoundException e) {
	        Log.w("Unable to get version code. Will not show changelog", e);
	        return -1;
	    }
	}
	
	private void showLog() {
	    LayoutInflater li = LayoutInflater.from(context);
	    View view = li.inflate(R.layout.changelog, null);

	    new AlertDialog.Builder(context)
	    	.setTitle("Changelog")
	    	.setIcon(android.R.drawable.ic_menu_info_details)
	    	.setView(view)
	    	.setNegativeButton("Dismiss", new DialogInterface.OnClickListener() {
	    		public void onClick(DialogInterface dialog, int whichButton) {
	    		}
	    	}).show();
	}
}
