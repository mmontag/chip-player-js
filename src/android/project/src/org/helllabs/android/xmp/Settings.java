package org.helllabs.android.xmp;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;

public class Settings extends android.preference.PreferenceActivity {
	public static final String PREF_MEDIA_PATH = "media_path";
	private SharedPreferences prefs;
	
    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        
        addPreferencesFromResource(R.xml.preferences);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
    }
}
