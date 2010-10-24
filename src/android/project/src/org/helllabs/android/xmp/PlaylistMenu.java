package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.List;

import android.app.ListActivity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public class PlaylistMenu extends ListActivity {
	static final int SETTINGS_REQUEST = 45;
	SharedPreferences prefs;
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist_menu);

		prefs = PreferenceManager.getDefaultSharedPreferences(this);
		setTitle("Playlists");
		
		registerForContextMenu(getListView());
		
		updateList();
	}
	
	void updateList() {
		String media_path = prefs.getString(Settings.PREF_MEDIA_PATH, Settings.DEFAULT_MEDIA_PATH);
		
		List<PlaylistInfo> list = new ArrayList<PlaylistInfo>();
		
		list.clear();
		list.add(new PlaylistInfo("Mod List", "Files in " + media_path));
		
		for (String p : PlayList.listNoSuffix()) {
			list.add(new PlaylistInfo(p, PlayList.readComment(this, p)));
		}
		
        final PlaylistInfoAdapter playlist = new PlaylistInfoAdapter(PlaylistMenu.this,
    			R.layout.playlist_item, R.id.plist_info, list);
        
        setListAdapter(playlist);
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		if (position == 0) {
			finish();
			return;
		} else {
			Intent intent = new Intent(PlaylistMenu.this, PlayList.class);
			intent.putExtra("name", PlayList.listNoSuffix()[position -1]);
			startActivity(intent);
		}
	}
	
	
	// Playlist context menu
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
	    AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)menuInfo;
		int i = 0;
		menu.setHeaderTitle("Playlist options");
		
		if (info.position == 0) {
			menu.add(Menu.NONE, i, i, "Change directory");
		} else {
			menu.add(Menu.NONE, i, i, "Delete");
		}
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
		int index = item.getItemId();

		if (info.position == 0) {
			if (index == 0) {
				// change directory
				return true;
			}
		} else {
			if (index == 0) {
				PlayList.deleteList(this, info.position - 1);
				updateList();
				return true;
			}			
		}

		return true;
	}
		
	@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	if (requestCode == SETTINGS_REQUEST) {
            if (resultCode == RESULT_OK) {
            	updateList();
                setResult(resultCode);
            }
        } else {
        	setResult(RESULT_CANCELED);
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
		case R.id.menu_refresh:
			updateList();
			break;
		}
		return true;
	}

}
