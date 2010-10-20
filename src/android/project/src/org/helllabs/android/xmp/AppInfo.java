package org.helllabs.android.xmp;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;

public class AppInfo {
	public static String getVersion(Context context) {
		try {
			PackageInfo packageInfo = context.getPackageManager().getPackageInfo(
						context.getPackageName(), 0);
			String version = packageInfo.versionName;
			int end = version.indexOf(' ');
			if (end > 0) {
				version = version.substring(0, end);
			}
			
			return version;
		} catch (NameNotFoundException e) {
			return "";
		}
	}
}
