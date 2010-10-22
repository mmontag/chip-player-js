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
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ListView;
import org.helllabs.android.xmp.R;

public class ModList extends ListActivity {
	static final int SETTINGS_REQUEST = 45;
	String media_path;
	List<ModInfo> modList = new ArrayList<ModInfo>();
	Xmp xmp = new Xmp();	/* used to get mod info */
	Button playAllButton, playListsButton, refreshButton;
	boolean single = false;		/* play only one module */
	boolean shuffleMode = true;
	boolean isBadDir = false;
	boolean firstTime = true;
	RandomIndex ridx;
	ProgressDialog progressDialog;
	SharedPreferences settings;
	final Handler handler = new Handler();
	
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	    	//Log.v(getString(R.string.app_name), "** " + dir + "/" + name);
	        return (xmp.testModule(dir + "/" + name) == 0);
	    }
	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.modlist);
		
		ChangeLog changeLog = new ChangeLog(this);
		
		settings = PreferenceManager.getDefaultSharedPreferences(this);
		
		xmp.initContext();
		
		playAllButton = (Button)findViewById(R.id.play_all);
		playListsButton = (Button)findViewById(R.id.playlists);
		refreshButton = (Button)findViewById(R.id.refresh);
		
		registerForContextMenu(getListView());
		
		playAllButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				playModule(modList);
			}
		});
		
		playListsButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {

		    }
		});
		
		refreshButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				updatePlaylist();
		    }
		});

		updatePlaylist();
		
		changeLog.show();
	}
	
	@Override
	public void onDestroy() {
		single = true;
		super.onDestroy();
	}
	
	public void updatePlaylist() {
		media_path = settings.getString(Settings.PREF_MEDIA_PATH, Settings.DEFAULT_MEDIA_PATH);
		//Log.v(getString(R.string.app_name), "path = " + media_path);

		modList.clear();
		
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
							settings.getBoolean(Settings.PREF_EXAMPLES, true));
					updatePlaylist();
				}
			});
			alertDialog.setButton2("Settings", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					startActivityForResult(new Intent(ModList.this, Settings.class), SETTINGS_REQUEST);
				}
			});
			alertDialog.show();
			return;
		}
		
		isBadDir = false;
		progressDialog = ProgressDialog.show(this,      
				firstTime ? "Xmp for Android" : "Please wait",
				"Scanning module files...", true);
		firstTime = false;
		
		new Thread() { 
			public void run() { 		
            	for (File file : modDir.listFiles(new ModFilter())) {
            		ModInfo m = xmp.getModInfo(media_path + "/" + file.getName());
            		modList.add(m);
            	}
            	
                final ModInfoAdapter playlist = new ModInfoAdapter(ModList.this,
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

	void playModule(List<ModInfo> list) {
		String[] mods = new String[list.size()];
		int i = 0;
		Iterator<ModInfo> element = list.iterator();
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
		Intent intent = new Intent(ModList.this, Player.class);
		intent.putExtra("files", mods);
		intent.putExtra("single", mode);
		startActivity(intent);
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		playModule(modList.get(position).filename);
	}
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
		int i = 0;
		menu.setHeaderTitle("Add to playlist");
		menu.add(Menu.NONE, i, i, "New playlist...");
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.options_menu, menu);
	    return true;
	}
	
	@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	//Log.v(getString(R.string.app_name), requestCode + ":" + resultCode);
    	if (requestCode == SETTINGS_REQUEST) {
            if (isBadDir || resultCode == RESULT_OK) {
            	updatePlaylist();
            }
        }
    }

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch(item.getItemId()) {
		case R.id.menu_formats:
			startActivity(new Intent(this, ListFormats.class));
			break;
		case R.id.menu_about:
			final AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			String version = "Version " + AppInfo.getVersion(this) + "\n\n";
			
			alertDialog.setIcon(R.drawable.icon);
			alertDialog.setTitle("Xmp for Android");
			alertDialog.setMessage(version + "Based on xmp " + xmp.getVersion() +
					" written by Claudio Matsuoka and Hipolito Carraro Jr" +
					"\n\nSupported module formats: " + xmp.getFormatCount() + "\n");
			alertDialog.setButton("Cool!", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					alertDialog.dismiss();
				}
			});
			alertDialog.show();	
			break;
		case R.id.menu_prefs:		
			startActivityForResult(new Intent(this, Settings.class), SETTINGS_REQUEST);
			/* Nicer, but only for API level 5
			overridePendingTransition(R.anim.slide_left, R.anim.slide_right);
			*/
			break;
		case R.id.menu_refresh:
			updatePlaylist();
			break;
		}
		return true;
	}
}
