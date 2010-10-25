package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

public class PlaylistUtils {
	ProgressDialog progressDialog;
	Xmp xmp = new Xmp();
	
	public void newPlaylist(Context context) {
		AlertDialog.Builder alert = new AlertDialog.Builder(context);		  
		alert.setTitle("New playlist");  	
	    LayoutInflater inflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
	    final View layout = inflater.inflate(R.layout.newlist, null);

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
		
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	        return (xmp.testModule(dir + "/" + name) == 0);
	    }
	}

	public void filesToPlaylist(final Context context, final String name) {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
		final String media_path = prefs.getString(Settings.PREF_MEDIA_PATH, Settings.DEFAULT_MEDIA_PATH);
		final File modDir = new File(media_path);
		
		progressDialog = ProgressDialog.show(context,      
				"Please wait", "Scanning module files...", true);
		
		new Thread() { 
			public void run() { 	
				List<String> list = new ArrayList<String>();
				
            	for (File file : modDir.listFiles(new ModFilter())) {
            		String filename = media_path + "/" + file.getName();
            		ModInfo mi = xmp.getModInfo(filename);
            		list.add(filename + ":" + mi.chn + " chn " + mi.type +
            				":" + mi.name);
            	}
            	
            	addToList(context, name + ".playlist", list.toArray(new String[1]));
            	
                progressDialog.dismiss();
			}
		}.start();
	}
	
	
	public void filesToNewPlaylist(final Context context, final Runnable runnable) {
		AlertDialog.Builder alert = new AlertDialog.Builder(context);		  
		alert.setTitle("New playlist");  	
	    LayoutInflater inflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
	    final View layout = inflater.inflate(R.layout.newlist, null);
	    final Handler handler = new Handler();

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
				
				filesToPlaylist(context, name);
				handler.post(runnable);
			}  
		});  
		  
		alert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {  
				// Canceled.  
			}  
		});  
		  
		alert.show();
	}

	
	public static void deleteList(Context context, int index) {
		String list = listNoSuffix()[index];
		(new File(Settings.dataDir, list + ".playlist")).delete();
		(new File(Settings.dataDir, list + ".comment")).delete();
	}
	
	
	public static void addToList(Context context, String name, String line) {
		File file = new File(Settings.dataDir, name);
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(file, true));
			out.write(line);
			out.newLine();
			out.close();
		} catch (IOException e) {
			Toast.makeText(context, "Error", Toast.LENGTH_SHORT).show();
		}
	}
	
	public static void addToList(Context context, String list, String[] lines) {
		File file = new File(Settings.dataDir, list);
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(file, true));
			for (String s : lines) {
				out.write(s);
				out.newLine();
			}
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
}
