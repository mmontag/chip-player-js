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
import android.media.AudioTrack;
import android.media.AudioManager;
import android.media.AudioFormat;
import org.helllabs.android.xmp.R;

public class ModPlayer extends ListActivity {
	private static final String MEDIA_PATH = new String("/sdcard/");
	private List<String> plist = new ArrayList<String>();
	private Xmp xmp = new Xmp();
	private AudioTrack audio = new AudioTrack(
			AudioManager.STREAM_MUSIC, 44100,
			AudioFormat.CHANNEL_CONFIGURATION_STEREO,
			AudioFormat.ENCODING_PCM_16BIT,
			1024, AudioTrack.MODE_STREAM);
	
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
   		if (xmp.load(MEDIA_PATH + plist.get(position)) < 0) {
   			xmp.deinit();
   			return;
   		}
   		xmp.play();
   		
   		while (xmp.playFrame() == 0) {
   			int size = xmp.softmixer();
   			short buffer[] = xmp.getBuffer(size);
   			audio.write(buffer, 0, size);
   		}
   		
   		xmp.stop();
   		xmp.release();
   		xmp.deinit();
	}
}
