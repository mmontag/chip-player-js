package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;


public class PlaylistUtils {
	ProgressDialog progressDialog;
	Xmp xmp = new Xmp();
	
	public void newPlaylist(final Context context) {
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
					FileUtils.writeToFile(file2, comment);
				} catch (IOException e) {
					Message.error(context, context.getString(R.string.error_create_playlist));
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
	        return InfoCache.testModule(dir + "/" + name);
	    }
	}

	public void filesToPlaylist(final Context context, final String path, final String name) {
		final File modDir = new File(path);
		
		if (!modDir.isDirectory()) {
			Message.error(context, context.getString(R.string.error_exist_dir));
			return;
		}
		
		progressDialog = ProgressDialog.show(context,      
				"Please wait", "Scanning module files...", true);
		
		new Thread() { 
			public void run() { 	
				List<String> list = new ArrayList<String>();
				
				int num = 0;
            	for (File file : modDir.listFiles(new ModFilter())) {
            		if (file.isDirectory())
            			continue;
            		String filename = path + "/" + file.getName();
            		ModInfo mi = InfoCache.getModInfo(filename);
            		list.add(filename + ":" + mi.chn + " chn " + mi.type +
            				":" + mi.name);
            		num++;
            	}
            	
            	if (num > 0)
            		addToList(context, name, list.toArray(new String[num]));
            	
                progressDialog.dismiss();
			}
		}.start();
	}
	
	
	public void filesToNewPlaylist(final Context context, final String path, final Runnable runnable) {
		final File modDir = new File(path);
		
		if (!modDir.isDirectory()) {
			Message.error(context, context.getString(R.string.error_exist_dir));
			return;
		}
		
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
					FileUtils.writeToFile(file2, comment);
				} catch (IOException e) {
					Message.error(context, context.getString(R.string.error_create_playlist));
				}
				
				filesToPlaylist(context, path, name);
				if (runnable != null)
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
		try {
			FileUtils.writeToFile(new File(Settings.dataDir, name + ".playlist"), line);
		} catch (IOException e) {
			Message.error(context, context.getString(R.string.error_write_to_playlist));
		}
			
	}
	
	public static void addToList(Context context, String name, String[] lines) {
		try {
			FileUtils.writeToFile(new File(Settings.dataDir, name + ".playlist"), lines);
		} catch (IOException e) {
			Message.error(context, context.getString(R.string.error_write_to_playlist));
		}
	}	
	
	public static String readComment(Context context, String name) {
		String comment = null;
		try {
			comment = FileUtils.readFromFile(new File(Settings.dataDir, name + ".comment"));
		} catch (IOException e) {
			Message.error(context, context.getString(R.string.error_read_comment));
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
