package org.helllabs.android.xmp;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.widget.EditText;
import android.widget.Toast;

class PlayListFilter implements FilenameFilter {
	public boolean accept(File dir, String name) {
		return name.endsWith(".playlist");
	}
}

public class PlayList extends ListActivity {
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist);
		
	}
	
	public static String[] list() {		
		return Settings.dataDir.list(new PlayListFilter());
	}
	
	public static void addNew(Context context) {
		AlertDialog.Builder alert = new AlertDialog.Builder(context);
		  
		alert.setTitle("New playlist");  
		alert.setMessage("Enter the new playlist name:");  
		     
		final EditText input = new EditText(context);  
		alert.setView(input);  
		  
		alert.setPositiveButton("Ok", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {  
				String value = input.getText().toString();
				File file = new File(Settings.dataDir, value + ".playlist");
				try {
					file.createNewFile();
				} catch (IOException e) {
					Toast.makeText(input.getContext(), "Error", Toast.LENGTH_SHORT)
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
	
	public static void addToList(Context context, String list, String line) {
		File file = new File(Settings.dataDir, list);
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(file));
			out.write(line + "\n");
			out.close();
		} catch (IOException e) {
			Toast.makeText(context, "Error", Toast.LENGTH_SHORT).show();
		}
	}
}
