package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.List;

import android.app.ListActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;

public class PlaylistMenu extends ListActivity {
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist_menu);
		
		setTitle("Playlists");
		
		List<PlaylistInfo> list = new ArrayList<PlaylistInfo>();
		
		list.clear();
		list.add(new PlaylistInfo("Mod List", "Comment"));
		
		for (String p : PlayList.listNoSuffix()) {
			list.add(new PlaylistInfo(p, "Comment"));
		}
		
        final PlaylistInfoAdapter playlist = new PlaylistInfoAdapter(PlaylistMenu.this,
    			R.layout.playlistmenu_item, R.id.plist_info, list);
        
        setListAdapter(playlist);
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		if (position == 0) {
			finish();
			return;
		}
	}

}
