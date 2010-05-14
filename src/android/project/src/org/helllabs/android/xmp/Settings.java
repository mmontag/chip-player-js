package org.helllabs.android.xmp;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.KeyEvent;

public class Settings extends android.preference.PreferenceActivity {
	public static final String PREF_MEDIA_PATH = "media_path";
	private SharedPreferences prefs;
	private String oldPath;
	
    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        oldPath = prefs.getString(PREF_MEDIA_PATH, null);    
        addPreferencesFromResource(R.xml.preferences);
    }
        
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	if(event.getAction() == KeyEvent.ACTION_DOWN) {
    		switch(keyCode) {
    		case KeyEvent.KEYCODE_BACK:
    			String newPath = prefs.getString(PREF_MEDIA_PATH, null);
    			setResult(newPath == oldPath ? RESULT_CANCELED : RESULT_OK);
    			finish();
    		}
    	}

    	return super.onKeyDown(keyCode, event);
    }
}
