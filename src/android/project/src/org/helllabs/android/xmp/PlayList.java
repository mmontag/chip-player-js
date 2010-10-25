package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import android.content.Context;
import android.os.Bundle;
import android.widget.Toast;

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

		updateList();
	}
	
	void updateList() {
		modList.clear();
		
		File file = new File(Settings.dataDir, name + ".playlist");
		String line;
		
	    try {
	    	BufferedReader in = new BufferedReader(new FileReader(file));
	    	while ((line = in.readLine()) != null) {
	    		String[] fields = line.split(":", 3);
	    		modList.add(new PlaylistInfo(fields[2], fields[1], fields[0]));
	    	}
	    	in.close();
	    } catch (IOException e) {
	    	 
	    }		
		
	    final PlaylistInfoAdapter plist = new PlaylistInfoAdapter(PlayList.this,
    			R.layout.playlist_item, R.id.plist_info, modList);
        
	    setListAdapter(plist);
	}
}
