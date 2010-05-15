/* Xmpoid.java
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
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.animation.AnimationUtils;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.ViewFlipper;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.helllabs.android.xmp.R;

public class Xmpoid extends ListActivity {
	private static final int SETTINGS_REQUEST = 45;
	private String media_path;
	private List<ModInfo> modList = new ArrayList<ModInfo>();
	private Xmp xmp = new Xmp();	/* used to get mod info */
	private ModPlayer player;		/* actual mod player */ 
	private ImageButton playButton, stopButton, backButton, forwardButton;
	private SeekBar seekBar;
	private boolean playing = false;
	private boolean seeking = false;
	private boolean single = false;		/* play only one module */
	private boolean shuffleMode = true;
	private boolean paused = false;
	private boolean isBadDir = false;
	private boolean firstTime = true;
	private ViewFlipper flipper;
	private TextView infoName, infoType, infoLen, infoTime;
	private TextView infoNpat, infoChn, infoIns, infoSmp;
	private TextView infoTpo, infoBpm, infoPos, infoPat; 
	private int playIndex;
	private RandomIndex ridx;
	private ProgressDialog progressDialog;
	private SharedPreferences settings;
	final Handler handler = new Handler();
	
    final Runnable endSongRunnable = new Runnable() {
        public void run() {
    		if (!single) {
    			if (++playIndex >= modList.size()) {
    				ridx.randomize();
    				playIndex = 0;
    			}
    			playNewMod(playIndex);
    		} else {
    			flipper.setAnimation(AnimationUtils.loadAnimation(flipper.getContext(), R.anim.slide_right));
    			flipper.showPrevious();
    			playButton.setImageResource(R.drawable.play);
    			playing = false;
    		}
        }
    };
    
    final Runnable updateInfoRunnable = new Runnable() {
    	private int oldTpo = -1;
    	private int oldBpm = -1;
    	private int oldPos = -1;
    	private int oldPat = -1;
        public void run() {
        	int tpo = player.getPlayTempo();
        	if (tpo != oldTpo) {
        		infoTpo.setText(Integer.toString(tpo));
        		oldTpo = tpo;
        	}

        	int bpm = player.getPlayBpm();
        	if (bpm != oldBpm) {
        		infoBpm.setText(Integer.toString(bpm));
        		oldBpm = bpm;
        	}

        	int pos = player.getPlayPos();
        	if (pos != oldPos) {
        		infoPos.setText(Integer.toString(pos));
        		oldPos = pos;
        	}

        	int pat = player.getPlayPat();
        	if (pat != oldPat) {
        		infoPat.setText(Integer.toString(pat));
        		oldPat = pat;
        	}        	
        }
    };
    
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
			int count = 0;		/* to reduce CPU usage */
    		int t = 0;
    		
    		do {
    			t = player.time();
    			//Log.v(getString(R.string.app_name), "t = " + t);
    			if (t >= 0) {
    				if (!seeking && !paused)
    					seekBar.setProgress(t);
    			}
    			
    			try {
					sleep(250);
				} catch (InterruptedException e) {
					Log.e(getString(R.string.app_name), e.getMessage());
				}
				
				if (count == 0)
					handler.post(updateInfoRunnable);
				if (++count > 1)
					count = 0;
    		} while (t >= 0);
    		
    		seekBar.setProgress(0);
    		handler.post(endSongRunnable);
    	}
    };

    private ProgressThread progressThread;
    
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	    	//Log.v(getString(R.string.app_name), "** " + dir + "/" + name);
	        return (xmp.testModule(dir + "/" + name) == 0);
	    }
	}
	
	public void pause() {
		paused = true;
		playButton.setImageResource(R.drawable.play);
	}
	
	public void unpause() {
		paused = false;
		playButton.setImageResource(R.drawable.pause);
	}
    
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist);
		
		settings = PreferenceManager.getDefaultSharedPreferences(this);

		player = new ModPlayer();
		
		/* Info view widgets */
		flipper = (ViewFlipper)findViewById(R.id.flipper);
		infoName = (TextView)findViewById(R.id.info_name);
		infoType = (TextView)findViewById(R.id.info_type);
		infoLen = (TextView)findViewById(R.id.info_len);
		infoNpat = (TextView)findViewById(R.id.info_npat);
		infoChn = (TextView)findViewById(R.id.info_chn);
		infoIns = (TextView)findViewById(R.id.info_ins);
		infoSmp = (TextView)findViewById(R.id.info_smp);
		infoTime = (TextView)findViewById(R.id.info_time);
		infoTpo = (TextView)findViewById(R.id.info_tpo);
		infoBpm = (TextView)findViewById(R.id.info_bpm);
		infoPos = (TextView)findViewById(R.id.info_pos);
		infoPat = (TextView)findViewById(R.id.info_pat);
		
		playButton = (ImageButton)findViewById(R.id.play);
		stopButton = (ImageButton)findViewById(R.id.stop);
		backButton = (ImageButton)findViewById(R.id.back);
		forwardButton = (ImageButton)findViewById(R.id.forward);
		
		playButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				synchronized (this) {
					if (playing) {
						player.pause();
					
						if (paused) {
							unpause();
						} else {
							pause();
						}
					
						return;
					}
				
					playing = true;
				}
								
				int idx[] = new int[modList.size()];
				for (int i = 0; i < modList.size(); i++) {
					idx[i] = i;
				}
				
				ridx = new RandomIndex(idx.length);				
				single = false;
				
				flipper.setAnimation(AnimationUtils.loadAnimation(v.getContext(), R.anim.slide_left));				
				flipper.showNext();
				playIndex = 0;
				unpause();
				playNewMod(0);
		    }
		});
		
		stopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				stopPlayingMod();
		    }
		});
		
		backButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				if (playing && !single) {
					playIndex -= 2;
					if (playIndex < -1)
						playIndex = -1;
					player.stop();
					unpause();
				}
		    }
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				if (playing && !single) {
					player.stop();
					unpause();
				}
		    }
		});
		
		seekBar = (SeekBar)findViewById(R.id.seek);
		seekBar.setProgress(0);
		
		seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar s, int p, boolean b) {
				// do nothing
			}

			public void onStartTrackingTouch(SeekBar s) {
				seeking = true;
			}

			public void onStopTrackingTouch(SeekBar s) {
				
				if (!playing) {
					seekBar.setProgress(0);
					return;
				}
				
				player.seek(s.getProgress());
				seeking = false;
			}
		});
		
		updatePlaylist();
	}
	
	@Override
	public void onDestroy() {
		single = true;
		player.stop();
		super.onDestroy();
	}
	
	public void updatePlaylist() {
		stopPlayingMod();
		
		media_path = settings.getString(Settings.PREF_MEDIA_PATH, "/sdcard/mod");
		//Log.v(getString(R.string.app_name), "path = " + media_path);

		modList.clear();
		
		final File modDir = new File(media_path);
		
		if (!modDir.isDirectory()) {
			isBadDir = true;
			AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setTitle("Oops");
			alertDialog.setMessage(media_path + " not found. " +
					"Create this directory or change the module path.");
			alertDialog.setButton("Bummer!", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					finish();
				}
			});
			alertDialog.setButton2("Settings", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					startActivityForResult(new Intent(Xmpoid.this, Settings.class), SETTINGS_REQUEST);
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
            	
                final ModInfoAdapter playlist = new ModInfoAdapter(Xmpoid.this,
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

	void playNewMod(int position)
	{
		/* Sanity check */
		if (position < 0 || position >= modList.size())
			position = 0;
						
		if (shuffleMode && !single)
			position = ridx.getIndex(position);

		ModInfo m = modList.get(position);
       	seekBar.setProgress(0);
       	seekBar.setMax(m.time / 100);
        	
       	infoName.setText(m.name);
       	infoType.setText(m.type);
       	infoLen.setText(Integer.toString(m.len));
       	infoNpat.setText(Integer.toString(m.pat));
       	infoChn.setText(Integer.toString(m.chn));
       	infoIns.setText(Integer.toString(m.ins));
       	infoSmp.setText(Integer.toString(m.smp));
       	infoTime.setText(Integer.toString((m.time + 500) / 60000) + "min" + 
       			Integer.toString(((m.time + 500) / 1000) % 60) + "s");
        	
       	player.play(m.filename);
        progressThread = new ProgressThread();
        progressThread.start();	
	}
	
	void stopPlayingMod() {
		if (!playing)
			return;
		
		single = true;
		player.stop();
		paused = false;
		playButton.setImageResource(R.drawable.play);
		
		try {
			progressThread.join();
		} catch (InterruptedException e) {
			Log.e(getString(R.string.app_name), e.getMessage());
		}
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		synchronized (this) {
			if (playing)
				return;
			playing = true;
		}
		single = true;
		flipper.setAnimation(AnimationUtils.loadAnimation(v.getContext(), R.anim.slide_left));
		flipper.showNext();
		unpause();
		playNewMod(position);
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
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	if(event.getAction() == KeyEvent.ACTION_DOWN) {
    		switch(keyCode) {
    		case KeyEvent.KEYCODE_BACK:
    			if (playing) {
    				stopPlayingMod();
    			} else {
    				finish();
    			}
    			return true;
    		}
    	}
    	
    	return super.onKeyDown(keyCode, event);
    }

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch(item.getItemId()) {
		case R.id.menu_quit:
			try {
				finish();
			} catch (Throwable e) {
				Log.e(getString(R.string.app_name), e.getMessage());
			}
			break;
		case R.id.menu_about:
			final AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setIcon(R.drawable.icon);
			alertDialog.setTitle(getString(R.string.app_name) + " " + getString(R.string.app_version));
			alertDialog.setMessage("Based on xmp " + xmp.getVersion() +
					" written by Claudio Matsuoka & Hipolito Carraro Jr" +
					"\n\nSupported module formats: " + xmp.getFormatCount() +
					"\n\nSee http://xmp.sf.net for more information and source code");
			alertDialog.setButton("Cool!", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					alertDialog.dismiss();
				}
			});
			alertDialog.show();	
			break;
		case R.id.menu_prefs:		
			startActivityForResult(new Intent(this, Settings.class), SETTINGS_REQUEST);
			/* Nicer, but only for API level 5 :(
			overridePendingTransition(int R.anim.slide_left, int R.anim.slide_right);
			*/
			break;
		case R.id.menu_refresh:
			updatePlaylist();
			break;
		}
		return true;
	}
}
