package org.helllabs.android.xmp;

import java.io.File;
//import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import android.app.ListActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import org.helllabs.android.xmp.R;

public class ModPlayer extends ListActivity {
	private static final String MEDIA_PATH = new String("/sdcard");
	private List<String> plist = new ArrayList<String>();
	private Xmp xmp = new Xmp();
	
	@Override
	public void onCreate(Bundle icicle) {
		try {
			super.onCreate(icicle);
			setContentView(R.layout.playlist);
			updatePlaylist();
		} catch (NullPointerException e) {
			Log.v(getString(R.string.app_name), e.getMessage());
		}
	}

	public void updatePlaylist() {
		File home = new File(MEDIA_PATH);
		for (File file : home.listFiles()) {
			plist.add(file.getName());
		}
		
        ArrayAdapter<String> playlist = new ArrayAdapter<String>(this,
        			R.layout.song_item, plist);
        setListAdapter(playlist);
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		/* FIXME: check exception */
   		xmp.init();
   		xmp.load(MEDIA_PATH + plist.get(position));
   		xmp.play();
   		
   		xmp.stop();
   		xmp.release();
   		xmp.deinit();
	}
}
