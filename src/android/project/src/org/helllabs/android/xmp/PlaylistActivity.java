package org.helllabs.android.xmp;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import android.app.ListActivity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.ListView;

public class PlaylistActivity extends ListActivity {
	List<PlaylistInfo> modList = new ArrayList<PlaylistInfo>();
	ImageButton playAllButton, toggleLoopButton, toggleShuffleButton;
	boolean shuffleMode = true;
	boolean loopMode = false;
	SharedPreferences prefs;
	boolean showToasts;
	ModInterface modPlayer;
	String[] addList;
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		
		prefs = PreferenceManager.getDefaultSharedPreferences(this);
		showToasts = prefs.getBoolean(Settings.PREF_SHOW_TOAST, true);
	
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
				if (showToasts)
					Message.toast(v.getContext(), loopMode ? "Loop on" : "Loop off");
		    }
		});

		toggleShuffleButton.setImageResource(shuffleMode ?
				R.drawable.list_shuffle_on : R.drawable.list_shuffle_off);
		toggleShuffleButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				shuffleMode = !shuffleMode;
				((ImageButton)v).setImageResource(shuffleMode ?
						R.drawable.list_shuffle_on : R.drawable.list_shuffle_off);
				if (showToasts)
					Message.toast(v.getContext(), shuffleMode ? "Shuffle on" : "Shuffle off");				
		    }
		});
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		playModule(modList.get(position).filename);
	}

	void playModule(List<PlaylistInfo> list) {
		int num = 0;
		for (PlaylistInfo p : list) {
			if ((new File(p.filename).isFile()))
				num++;
		}
		if (num == 0)
			return;
		
		String[] mods = new String[num];
		int i = 0;
		for (PlaylistInfo p : list) {
			if ((new File(p.filename)).isFile()) {
				mods[i++] = p.filename;
			}
		}
		if (i > 0) {
			playModule(mods);
		}
	}
	
	void playModule(String mod) {
		String[] mods = { mod };
		playModule(mods);
	}
	
	void playModule(String[] mods) {
		if (showToasts) {
			if (mods.length > 1)
				Message.toast(this, "Play all modules in list");
			else
				Message.toast(this, "Play only this module");
		}
		Intent intent = new Intent(this, Player.class);
		intent.putExtra("files", mods);
		intent.putExtra("shuffle", shuffleMode);
		intent.putExtra("loop", loopMode);
		startActivity(intent);
	}
	
	// Connection
	
	private ServiceConnection connection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			modPlayer = ModInterface.Stub.asInterface(service);
			try {				
				modPlayer.add(addList);
			} catch (RemoteException e) {
				Message.toast(PlaylistActivity.this, "Error adding module");
			}
			unbindService(connection);
		}

		public void onServiceDisconnected(ComponentName className) {
			modPlayer = null;
		}
	};
	
	protected void addToQueue(int start, int size) {
		String[] list = new String[size];
		
		for (int i = 0; i < size; i++)
			list[i] = modList.get(start + i).filename;
		
		Intent service = new Intent(this, ModService.class);
		
		if (ModService.isPlaying) {
			addList = list;
			bindService(service, connection, 0);
		} else {
    		playModule(list);
    	}
	}
}
