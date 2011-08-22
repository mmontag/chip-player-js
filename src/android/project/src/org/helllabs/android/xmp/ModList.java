/* Xmp for Android
 * Copyright (C) 2010 Claudio Matsuoka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

package org.helllabs.android.xmp;

import java.io.File;
import java.io.FileFilter;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public class ModList extends PlaylistActivity {
	Xmp xmp = new Xmp();	/* used to get mod info */
	boolean isBadDir = false;
	ProgressDialog progressDialog;
	final Handler handler = new Handler();
	String currentDir;
	int directoryNum;
	int parentNum;
	int playlistSelection;
	int fileSelection;
	int fileNum;
	Context context;
	
	class DirFilter implements FileFilter {
	    public boolean accept(File dir) {
	        return dir.isDirectory();
	    }
	}
	
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	    	File f = new File(dir,name);
	        return !f.isDirectory() && InfoCache.testModule(f.getPath());
	    }
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		setContentView(R.layout.modlist);
		super.onCreate(icicle);			
		registerForContextMenu(getListView());
		final String media_path = prefs.getString(Settings.PREF_MEDIA_PATH, Settings.DEFAULT_MEDIA_PATH);
		
		context = this;
		
		// Check if directory exists
		final File modDir = new File(media_path);
		
		if (!modDir.isDirectory()) {
			final Examples examples = new Examples(this);
			
			isBadDir = true;
			AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setTitle("Oops");
			alertDialog.setMessage(media_path + " not found. " +
					"Create this directory or change the module path.");
			alertDialog.setButton("Create", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					examples.install(media_path,
							prefs.getBoolean(Settings.PREF_EXAMPLES, true));
					updateModlist(media_path);
				}
			});
			alertDialog.setButton2("Back", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					finish();
				}
			});
			alertDialog.show();
			return;
		}
		
		updateModlist(media_path);
	}

	public void update() {
		updateModlist(currentDir);
	}
	
	public void updateModlist(final String path) {
		modList.clear();
		
		currentDir = path;
		setTitle(path);
		
		isBadDir = false;
		progressDialog = ProgressDialog.show(this,      
				"Please wait", "Scanning module files...", true);

		parentNum = directoryNum = 0;
		final File modDir = new File(path);
		new Thread() { 
			public void run() {
				if (!path.equals("/")) {
					modList.add(new PlaylistInfo("..", "Parent directory", path + "/..", R.drawable.parent));
					parentNum++;
					directoryNum++;
				}	
				
				List<PlaylistInfo> list = new ArrayList<PlaylistInfo>();
            	for (File file : modDir.listFiles(new DirFilter())) {
            		directoryNum++;
            		list.add(new PlaylistInfo(file.getName(), "Directory",
            						file.getAbsolutePath(), R.drawable.folder));
            	}
            	Collections.sort(list);
            	modList.addAll(list);
            	
            	list.clear();
            	for (File file : modDir.listFiles(new ModFilter())) {
            		ModInfo m = InfoCache.getModInfo(path + "/" + file.getName());
            		list.add(new PlaylistInfo(m.name, m.chn + " chn " + m.type, m.filename, -1));
            	}
            	Collections.sort(list);
            	modList.addAll(list);
            	
                final PlaylistInfoAdapter playlist = new PlaylistInfoAdapter(ModList.this,
                			R.layout.song_item, R.id.info, modList);
                
                /* This one must run in the UI thread */
                handler.post(new Runnable() {
                	public void run() {
                		 setListAdapter(playlist);
                	 }
                });
            	
                progressDialog.dismiss();
			}
		}.start();
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		String name = modList.get(position).filename;
		File file = new File(name);
		
		if (file.isDirectory()) {	
			if (file.getName().equals("..")) {
				if ((name = file.getParentFile().getParent()) == null)
					name = "/";
			}
			updateModlist(name);
		} else {
			playModule(name);
		}
	}
	
	@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	Log.i("Xmp ModList", "Activity result " + requestCode + "," + resultCode);
    	switch (requestCode) {
    	case SETTINGS_REQUEST:
            if (isBadDir || resultCode == RESULT_OK)
            	updateModlist(currentDir);
            super.showToasts = prefs.getBoolean(Settings.PREF_SHOW_TOAST, true);
            break;
    	case PLAY_MODULE_REQUEST:
    		if (resultCode != RESULT_OK)
    			updateModlist(currentDir);
    		break;
        }
    }

	
	// Playlist context menu
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)menuInfo;
		menu.setHeaderTitle("Add to playlist");
		if (info.position < parentNum) {
			// Do nothing
		} else if (info.position < directoryNum) {			// For directory
			menu.add(Menu.NONE, 0, 0, "Add to playlist");		
		} else {											// For files
			menu.add(1, 0, 0, "Add to playlist");
			menu.add(1, 1, 1, "Add all to playlist");
			menu.add(Menu.NONE, 2, 2, "Add to play queue");
			menu.add(Menu.NONE, 3, 3, "Add all to play queue");
			menu.add(Menu.NONE, 4, 4, "Delete file");
		}
		
		menu.setGroupEnabled(1, PlaylistUtils.list().length > 0);
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
		final int id = item.getItemId();

		if (info.position < parentNum) {					// Parent dir
			// Do nothing
		} else if (info.position < directoryNum) {			// Directories
			if (id == 0) {
				addToPlaylist(info.position, 1, addDirToPlaylistDialogClickListener);
			}
		} else {										// Files
			switch (id) {
			case 0:										// Add to playlist
				addToPlaylist(info.position, 1, addFileToPlaylistDialogClickListener);
				break;
			case 1:										// Add all to playlist
				addToPlaylist(directoryNum, modList.size() - directoryNum, addFileToPlaylistDialogClickListener);
				break;
			case 2:
				addToQueue(info.position, 1);
				break;
			case 3:
				addToQueue(directoryNum, modList.size() - directoryNum);
				break;
			case 4:
				deleteName = modList.get(info.position).filename;
				Message.yesNoDialog(this, R.string.msg_really_delete, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						if (which == DialogInterface.BUTTON_POSITIVE) {
							if (InfoCache.delete(deleteName)) {
								updateModlist(currentDir);
								Message.toast(context, getString(R.string.msg_file_deleted));
							} else {
								Message.toast(context, getString(R.string.msg_cant_delete));
							}
						}
					}
				});

				break;
			}
		}

		return true;
	}
	
	protected void addToPlaylist(int start, int num, DialogInterface.OnClickListener listener) {
		fileSelection = start;
		fileNum = num;
		playlistSelection = -1;
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle(R.string.msg_select_playlist)
		.setPositiveButton(R.string.msg_ok, listener)
	    .setNegativeButton(R.string.msg_cancel, listener)
	    .setSingleChoiceItems(PlaylistUtils.listNoSuffix(), 0, new DialogInterface.OnClickListener() {
		    public void onClick(DialogInterface dialog, int which) {
		        playlistSelection = which;
		    }
		})
	    .show();
	}	
	
	/*
	 * Add directory to playlist
	 */
	private DialogInterface.OnClickListener addDirToPlaylistDialogClickListener = new DialogInterface.OnClickListener() {
	    public void onClick(DialogInterface dialog, int which) {
	    	PlaylistUtils p = new PlaylistUtils();
	    	
	        switch (which) {
	        case DialogInterface.BUTTON_POSITIVE:
	        	if (playlistSelection >= 0) {
	        		p.filesToPlaylist(context, modList.get(fileSelection).filename,
	        					PlaylistUtils.listNoSuffix()[playlistSelection]);
	        	}
	            break;
	        case DialogInterface.BUTTON_NEGATIVE:
	            break;
	        }
	    }
	};
	
	/*
	 * Add Files to playlist
	 */	
	private DialogInterface.OnClickListener addFileToPlaylistDialogClickListener = new DialogInterface.OnClickListener() {
	    public void onClick(DialogInterface dialog, int which) {
	        switch (which) {
	        case DialogInterface.BUTTON_POSITIVE:
	        	if (playlistSelection >= 0) {
	        		for (int i = fileSelection; i < fileSelection + fileNum; i++) {
	        			PlaylistInfo pi = modList.get(i);
	        			String line = pi.filename + ":" + pi.comment + ":" + pi.name;
	        			PlaylistUtils.addToList(context, PlaylistUtils.listNoSuffix()[playlistSelection], line);
	        		}
	        	}
	            break;
	        case DialogInterface.BUTTON_NEGATIVE:
	            break;
	        }
	    }
	};
	
	protected void deleteFile(int start) {
		deleteName = modList.get(start).filename;
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setMessage(R.string.msg_really_delete)
			.setPositiveButton(R.string.msg_yes, deleteDialogClickListener)
		    .setNegativeButton(R.string.msg_no, deleteDialogClickListener)
		    .show();
		
	}
	
	private DialogInterface.OnClickListener deleteDialogClickListener = new DialogInterface.OnClickListener() {
	    public void onClick(DialogInterface dialog, int which) {
	        switch (which) {
	        case DialogInterface.BUTTON_POSITIVE:
	        	if (InfoCache.delete(deleteName)) {
	        		updateModlist(currentDir);
	        		Message.toast(context, getString(R.string.msg_file_deleted));
	        	} else {
	        		Message.toast(context, getString(R.string.msg_cant_delete));
	        	}
	            break;

	        case DialogInterface.BUTTON_NEGATIVE:
	            break;
	        }
	    }
	};
}
