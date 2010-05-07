package org.helllabs.android.xmp;

import java.io.File;
//import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import android.app.ListActivity;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.media.AudioTrack;
import android.media.AudioManager;
import android.media.AudioFormat;
import org.helllabs.android.xmp.R;


public class ModPlayer extends ListActivity {
	private class ModFile {
		String title;
		String filename;
		String format;
		int time;
	}
	
	private class ModFileAdapter extends ArrayAdapter<ModFile> {
	    private List<ModFile> items;

        public ModFileAdapter(Context context, int resource, int textViewResourceId, List<ModFile> items) {
                super(context, resource, textViewResourceId, items);
                this.items = items;
        }
        
        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
                View v = convertView;
                if (v == null) {
                    LayoutInflater vi = (LayoutInflater)getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                    v = vi.inflate(R.layout.song_item, null);
                }
                ModFile o = items.get(position);
                if (o != null) {
                        TextView tt = (TextView) v.findViewById(R.id.title);
                        TextView bt = (TextView) v.findViewById(R.id.info);
                        if (tt != null) {
                              tt.setText(o.title);                            }
                        if(bt != null){
                              bt.setText(o.filename);
                        }
                }
                
                return v;
        }
	}
	
	private static final String MEDIA_PATH = new String("/sdcard/");
	private List<ModFile> modlist = new ArrayList<ModFile>();
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
			ModFile m = new ModFile();
			m.filename = file.getName();
			modlist.add(m);
		}
		
        ModFileAdapter playlist = new ModFileAdapter(this,
        			R.layout.song_item, R.id.info, modlist);
        setListAdapter(playlist);
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		//Log.v(getString(R.string.app_name), "** start " + minSize);
		audio.play();
		
		/* FIXME: check exception */
   		xmp.init();
   		if (xmp.loadModule(MEDIA_PATH + modlist.get(position)) < 0) {
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
