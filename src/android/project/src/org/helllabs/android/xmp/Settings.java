package org.helllabs.android.xmp;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.KeyEvent;

public class Settings extends android.preference.PreferenceActivity {
	public static final String DEFAULT_MEDIA_PATH = "/sdcard/mod";
	public static final String PREF_MEDIA_PATH = "media_path";
	public static final String PREF_VOL_BOOST = "vol_boost";
	public static final String PREF_CHANGELOG_VERSION = "changelog_version";
	public static final String PREF_STEREO = "stereo";
	public static final String PREF_PAN_SEPARATION = "pan_separation";
	public static final String PREF_EXAMPLES = "examples";
	public static final String PREF_METERS = "meters";
	private SharedPreferences prefs;
	private String oldPath;
	
    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        oldPath = prefs.getString(PREF_MEDIA_PATH, DEFAULT_MEDIA_PATH);
        addPreferencesFromResource(R.xml.preferences);
    }
        
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	if(event.getAction() == KeyEvent.ACTION_DOWN) {
    		switch(keyCode) {
    		case KeyEvent.KEYCODE_BACK:
    			String newPath = prefs.getString(PREF_MEDIA_PATH, DEFAULT_MEDIA_PATH);
    			setResult(newPath == oldPath ? RESULT_CANCELED : RESULT_OK);
    			finish();
    		}
    	}

    	return super.onKeyDown(keyCode, event);
    }
}
