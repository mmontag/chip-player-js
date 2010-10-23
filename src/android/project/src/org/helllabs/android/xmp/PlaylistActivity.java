package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import android.app.ListActivity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.ListView;

public class PlaylistActivity extends ListActivity {
	List<PlaylistInfo> modList = new ArrayList<PlaylistInfo>();
	ImageButton playAllButton, toggleLoopButton, toggleShuffleButton;
	boolean single = false;		/* play only one module */
	boolean shuffleMode = true;
	boolean loopMode = false;
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
	
		playAllButton = (ImageButton)findViewById(R.id.play_all);
		toggleLoopButton = (ImageButton)findViewById(R.id.toggle_loop);
		toggleShuffleButton = (ImageButton)findViewById(R.id.toggle_shuffle);
			
		playAllButton.setImageResource(R.drawable.list_play);
		playAllButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				playModule(modList);
			}
		});
	
		toggleLoopButton.setImageResource(loopMode ?
				R.drawable.list_loop_on : R.drawable.list_loop_off);
		toggleLoopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				loopMode = !loopMode;
				((ImageButton)v).setImageResource(loopMode ?
						R.drawable.list_loop_on : R.drawable.list_loop_off);
		    }
		});

		toggleShuffleButton.setImageResource(shuffleMode ?
				R.drawable.list_shuffle_on : R.drawable.list_shuffle_off);
		toggleShuffleButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				shuffleMode = !shuffleMode;
				((ImageButton)v).setImageResource(shuffleMode ?
						R.drawable.list_shuffle_on : R.drawable.list_shuffle_off);
				
		    }
		});
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		playModule(modList.get(position).filename);
	}
	
	void playModule(List<PlaylistInfo> list) {
		String[] mods = new String[list.size()];
		int i = 0;
		Iterator<PlaylistInfo> element = list.iterator();
		while (element.hasNext()) {
			mods[i++] = element.next().filename;
		}
		playModule(mods, false);
	}
	
	void playModule(String mod) {
		String[] mods = new String[1];
		mods[0] = mod;
		playModule(mods, true);
	}
	
	void playModule(String[] mods, boolean mode) {
		Intent intent = new Intent(this, Player.class);
		intent.putExtra("files", mods);
		intent.putExtra("single", mode);
		intent.putExtra("shuffle", shuffleMode);
		intent.putExtra("loop", loopMode);
		startActivity(intent);
	}
}
