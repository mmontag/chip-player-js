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
		setTitle(name + " - " + readComment(this, name));

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

	
	// Static methods
	
	public static String[] list() {		
		return Settings.dataDir.list(new PlayListFilter());
	}
	
	public static String[] listNoSuffix() {
		String[] pList = list();
		for (int i = 0; i < pList.length; i++) {
			pList[i] = pList[i].substring(0, pList[i].lastIndexOf(".playlist"));
		}
		return pList;
	}
	
	public static void deleteList(Context context, int index) {
		String list = listNoSuffix()[index];
		(new File(Settings.dataDir, list + ".playlist")).delete();
		(new File(Settings.dataDir, list + ".comment")).delete();
	}
	
	
	public static void addToList(Context context, String list, String line) {
		File file = new File(Settings.dataDir, list);
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(file, true));
			out.write(line);
			out.newLine();
			out.close();
		} catch (IOException e) {
			Toast.makeText(context, "Error", Toast.LENGTH_SHORT).show();
		}
	}
	
	public static String readComment(Context context, String name) {
		File file = new File(Settings.dataDir, name + ".comment");
		String comment = null;
		
		try {
			BufferedReader in = new BufferedReader(new FileReader(file));
	    	comment = in.readLine();
	    	in.close();
	    } catch (IOException e) {
	    	 
	    }
	    
	    if (comment == null || comment.trim().length() == 0)
	    	comment = context.getString(R.string.no_comment);
		return comment;
		
	}
}
