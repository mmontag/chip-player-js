package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import android.os.Bundle;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;

class PlayListFilter implements FilenameFilter {
	public boolean accept(File dir, String name) {
		return name.endsWith(".playlist");
	}
}

public class PlayList extends PlaylistActivity {
	String name;
	
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
	
	void update() {
		updateList();
	}
	
	void updateList() {
		modList.clear();
		
		File file = new File(Settings.dataDir, name + ".playlist");
		String line;
		int lineNum;
		
		List<Integer> invalidList = new ArrayList<Integer>();
		
	    try {
	    	BufferedReader in = new BufferedReader(new FileReader(file), 512);
	    	lineNum = 0;
	    	while ((line = in.readLine()) != null) {
	    		String[] fields = line.split(":", 3);
	    		if (!InfoCache.fileExists(fields[0])) {
	    			invalidList.add(lineNum);
	    		} else {
	    			modList.add(new PlaylistInfo(fields[2], fields[1], fields[0], -1));
	    		}
	    		lineNum++;
	    	}
	    	in.close();
	    } catch (IOException e) {
	    	 
	    }		
		
	    if (!invalidList.isEmpty()) {
	    	int[] x = new int[invalidList.size()];
	    	Iterator<Integer> iterator = invalidList.iterator();
	    	for (int i = 0; i < x.length; i++)
	    		x[i] = iterator.next().intValue();
	    	
			try {
				FileUtils.removeLineFromFile(file, x);
			} catch (FileNotFoundException e) {

			} catch (IOException e) {

			}
		}
	    
	    final PlaylistInfoAdapter plist = new PlaylistInfoAdapter(PlayList.this,
    				R.layout.playlist_item, R.id.plist_info, modList,
    				prefs.getBoolean(Settings.PREF_USE_FILENAME, false));
        
	    setListAdapter(plist);
	}
	
	// Playlist context menu
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
		//int i = 0;
		menu.setHeaderTitle("Edit playlist");
		menu.add(Menu.NONE, 0, 0, "Remove from playlist");
		menu.add(Menu.NONE, 1, 1, "Add to play queue");
		menu.add(Menu.NONE, 2, 2, "Add all to play queue");
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
		int id = item.getItemId();
		
		switch (id) {
		case 0:
			removeFromPlaylist(name, info.position);
			updateList();
			break;
		case 1:
			addToQueue(info.position, 1);
			break;
		case 2:
			addToQueue(0, modList.size());
	    	break;
		}

		return true;
	}
	
	public void removeFromPlaylist(String playlist, int position) {
		File file = new File(Settings.dataDir, name + ".playlist");
		try {
			FileUtils.removeLineFromFile(file, position);
		} catch (FileNotFoundException e) {

		} catch (IOException e) {

		}
	}
}
