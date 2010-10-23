package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.List;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public class PlaylistMenu extends ListActivity {
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist_menu);
		
		setTitle("Playlists");
		
		registerForContextMenu(getListView());
		
		updateList();
	}
	
	void updateList() {
		List<PlaylistInfo> list = new ArrayList<PlaylistInfo>();
		
		list.clear();
		list.add(new PlaylistInfo("Mod List", "Comment"));
		
		for (String p : PlayList.listNoSuffix()) {
			list.add(new PlaylistInfo(p, "Comment"));
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
}
