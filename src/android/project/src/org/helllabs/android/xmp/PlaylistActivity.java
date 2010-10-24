package org.helllabs.android.xmp;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.Toast;

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
	
	public void newPlaylist(Context context) {
		AlertDialog.Builder alert = new AlertDialog.Builder(context);		  
		alert.setTitle("New playlist");  	
	    LayoutInflater inflater = (LayoutInflater)context.getSystemService(LAYOUT_INFLATER_SERVICE);
	    final View layout = inflater.inflate(R.layout.newlist, (ViewGroup)findViewById(R.id.layout_root));

	    alert.setView(layout);
		  
		alert.setPositiveButton("Ok", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {
				EditText e1 = (EditText)layout.findViewById(R.id.new_playlist_name);
				EditText e2 = (EditText)layout.findViewById(R.id.new_playlist_comment);
				String name = e1.getText().toString();
				String comment = e2.getText().toString();
				File file1 = new File(Settings.dataDir, name + ".playlist");
				File file2 = new File(Settings.dataDir, name + ".comment");
				try {
					file1.createNewFile();
					file2.createNewFile();
					BufferedWriter out = new BufferedWriter(new FileWriter(file2));
					out.write(comment);
					out.close();
				} catch (IOException e) {
					Toast.makeText(e1.getContext(), "Error", Toast.LENGTH_SHORT)
						.show();
				}
			}  
		});  
		  
		alert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {  
				// Canceled.  
			}  
		});  
		  
		alert.show(); 
	}

}
