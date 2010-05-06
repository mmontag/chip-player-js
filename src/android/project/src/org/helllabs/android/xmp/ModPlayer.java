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
	private int minSize = AudioTrack.getMinBufferSize(44100,
			AudioFormat.CHANNEL_CONFIGURATION_STEREO,
			AudioFormat.ENCODING_PCM_16BIT);
	private AudioTrack audio = new AudioTrack(
			AudioManager.STREAM_MUSIC, 44100,
			AudioFormat.CHANNEL_CONFIGURATION_STEREO,
			AudioFormat.ENCODING_PCM_16BIT,
			minSize < 4096 ? 4096 : minSize,
			AudioTrack.MODE_STREAM);
	
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
		//Log.v(getString(R.string.app_name), "** start " + minSize);
		audio.play();
		
		/* FIXME: check exception */
   		xmp.init();
   		if (xmp.loadModule(MEDIA_PATH + plist.get(position)) < 0) {
   			xmp.deinit();
   			return;
   		}
   		xmp.startPlayer();
   		
   		while (xmp.playFrame() == 0) {
   			int size = xmp.softmixer();
   			short buffer[] = xmp.getBuffer(size);
   			int i = audio.write(buffer, 0, size / 2);
   			//Log.v(getString(R.string.app_name), "--> " + size + " " + i);
   		}
   		
   		audio.stop();
   		
   		xmp.endPlayer();
   		xmp.releaseModule();
   		xmp.deinit();
	}
}
