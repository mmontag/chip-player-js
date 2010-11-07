package org.helllabs.android.xmp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class About extends Activity {
	
	@Override
	public void onCreate(Bundle icicle) {	
		super.onCreate(icicle);
		
		setContentView(R.layout.about);

		Xmp xmp = new Xmp();
		
		((TextView)findViewById(R.id.version_name))
			.setText(getString(R.string.about_version, AppInfo.getVersion(this)));
		
		((TextView)findViewById(R.id.xmp_version))
			.setText(getString(R.string.about_xmp, xmp.getVersion()));
	}
		

}
