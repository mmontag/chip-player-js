package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;

class PlayListFilter implements FilenameFilter {
	public boolean accept(File dir, String name) {
		return name.endsWith(".playlist");
	}
}

public class PlayList extends PlaylistActivity {
	static final int SETTINGS_REQUEST = 45;
	String name;
	ModInterface modPlayer;
	int addPosition;
	
	@Override
	public void onCreate(Bundle icicle) {	
		setContentView(R.layout.playlist);
		super.onCreate(icicle);
		
		Bundle extras = getIntent().getExtras();
		if (extras == null)
			return;
		
		name = extras.getString("name");
		setTitle(name + " - " + PlaylistUtils.readComment(this, name));
		registerForContextMenu(getListView());

		updateList();
	}
	
	void updateList() {
		modList.clear();
		
		File file = new File(Settings.dataDir, name + ".playlist");
		String line;
		
	    try {
	    	BufferedReader in = new BufferedReader(new FileReader(file), 512);
	    	while ((line = in.readLine()) != null) {
	    		String[] fields = line.split(":", 3);
	    		modList.add(new PlaylistInfo(fields[2], fields[1], fields[0], -1));
	    	}
	    	in.close();
	    } catch (IOException e) {
	    	 
	    }		
		
	    final PlaylistInfoAdapter plist = new PlaylistInfoAdapter(PlayList.this,
    			R.layout.playlist_item, R.id.plist_info, modList);
        
	    setListAdapter(plist);
	}
	
	@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	//Log.v(getString(R.string.app_name), requestCode + ":" + resultCode);
    	switch (requestCode) {
    	case SETTINGS_REQUEST:
    		super.showToasts = prefs.getBoolean(Settings.PREF_SHOW_TOAST, true);
            break;
        }
    }
	
	
	// Playlist context menu
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
		//int i = 0;
		menu.setHeaderTitle("Edit playlist");
		menu.add(Menu.NONE, 0, 0, "Remove from playlist");
		menu.add(Menu.NONE, 1, 1, "Add to play queue");
		// TODO: find a good way to not list current playlist
		/*for (String each : PlaylistUtils.listNoSuffix()) {
			i++;
			menu.add(Menu.NONE, i, i, "Copy to " + each);
		}*/
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
		int index = item.getItemId();
		
		Log.i("asd", "index = " + index);
		switch (index) {
		case 0:
			removeFromPlaylist(name, info.position);
			updateList();
			break;
		case 1:
			Intent service = new Intent(this, ModService.class);
			addPosition = info.position;
			if (ModService.isPlaying) {
				Log.i("asd", "will bind");
				bindService(service, connection, 0);
			} else {
	    		Log.i("asd", "play " + modList.get(info.position).filename);
	    		playModule(modList.get(info.position).filename);
	    	}
	    	break;
		}
		/*
		index--;
		String[] menuItems = PlaylistUtils.list();
		PlaylistInfo pi = modList.get(info.position);
		String line = pi.filename + ":" + pi.comment + ":" + pi.name;
		PlaylistUtils.addToList(this, PlaylistUtils.listNoSuffix()[index], line);*/

		return true;
	}
	
	private ServiceConnection connection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			modPlayer = ModInterface.Stub.asInterface(service);
			String filename = modList.get(addPosition).filename;
			Log.i("Xmp PlayList", "Add " + filename + " to player queue");
			String[] files = { filename };
			try {				
				modPlayer.add(files);
			} catch (RemoteException e) {
				Message.toast(PlayList.this, "Error adding module");
			}
		}

		public void onServiceDisconnected(ComponentName className) {
			modPlayer = null;
		}
	};
		
	public void removeFromPlaylist(String playlist, int position) {
		File file = new File(Settings.dataDir, name + ".playlist");
		try {
			FileUtils.removeLineFromFile(file, position);
		} catch (FileNotFoundException e) {

		} catch (IOException e) {

		}
	}

	
	// Menu
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.playlist_menu, menu);
	    return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch(item.getItemId()) {
		case R.id.menu_prefs:		
			startActivityForResult(new Intent(this, Settings.class), SETTINGS_REQUEST);
			break;
		}
		return true;
	}	
}
