/* Copyright (C) 2010 Claudio Matsuoka
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
import java.util.Date;
import java.util.List;
import java.util.Random;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.view.animation.AnimationUtils;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.ViewFlipper;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.helllabs.android.xmp.R;


public class Xmpoid extends ListActivity {
	
	private class ModInfoAdapter extends ArrayAdapter<ModInfo> {
	    private List<ModInfo> items;

        public ModInfoAdapter(Context context, int resource, int textViewResourceId, List<ModInfo> items) {
        	super(context, resource, textViewResourceId, items);
        	this.items = items;
        }
        
        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
        	View v = convertView;
        	if (v == null) {
        		LayoutInflater vi = (LayoutInflater)getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        		v = vi.inflate(R.layout.song_item, null);
        	}
        	ModInfo o = items.get(position);
                
        	if (o != null) {                		
        		TextView tt = (TextView) v.findViewById(R.id.title);
        		TextView bt = (TextView) v.findViewById(R.id.info);
        		if (tt != null) {
        			tt.setText(o.name);
        		}
        		if(bt != null){
        			bt.setText(o.chn + " chn " + o.type);
        		}
        	}
                
        	return v;
        }
	}
	
	static final String MEDIA_PATH = new String("/sdcard/mod/");
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
	private ViewFlipper flipper;
	private TextView infoName, infoType, infoLen;
	private TextView infoNpat, infoChn, infoIns, infoSmp;
	private TextView infoTpo, infoBpm, infoPos, infoPat; 
	private int playIndex;
	private RandomIndex ridx;
	private ProgressDialog progressDialog;
	final Handler handler = new Handler();
	
	private class RandomIndex {
		private int[] idx;
		
		public RandomIndex(int n) {
			idx = new int[n];
			for (int i = 0; i < n; i++) {
				idx[i] = i;
			}
			
			randomize();
		}
	
		public void randomize() {
			Random random = new Random();
			Date date = new Date();
			random.setSeed(date.getTime());
			for (int i = 0; i < idx.length; i++) {				
				int r = random.nextInt(idx.length);
				int temp = idx[i];
				idx[i] = idx[r];
				idx[r] = temp;
			}
		}
		
		public int getIndex(int n) {
			return idx[n];
		}
	}
	
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
			playing = true;
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
    		playing = false;
    		
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
	public void onCreate(Bundle icicle) {
		xmp.init();
		
		super.onCreate(icicle);
		setContentView(R.layout.playlist);
		
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
				if (playing) {
					player.pause();
					
					if (paused) {
						unpause();
					} else {
						pause();
					}
					
					return;
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

	public void updatePlaylist() {
		final File modDir = new File(MEDIA_PATH);
		
		if (!modDir.isDirectory()) {
			AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setTitle("Oops");
			alertDialog.setMessage(MEDIA_PATH + " not found. " +
					"Create this directory and place your modules there.");
			alertDialog.setButton("Bummer!", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					finish();
				}
			});
			alertDialog.show();
			return;
		}
		
		progressDialog = ProgressDialog.show(this,      
				"Please wait", "Scanning module files...", true);
		
		new Thread() { 
			public void run() { 		
            	for (File file : modDir.listFiles(new ModFilter())) {
            		ModInfo m = xmp.getModInfo(MEDIA_PATH + file.getName());
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
		try {
			if (playing) {
				seeking = true;		/* To stop progress bar update */
				player.stop();
				progressThread.join();
				seeking = false;
			}

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
        	
            player.play(m.filename);
            progressThread = new ProgressThread();
            progressThread.start();
            
		} catch (InterruptedException e) {
			Log.e(getString(R.string.app_name), e.getMessage());
		}		
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
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
	public boolean onOptionsItemSelected(MenuItem item) {
		switch(item.getItemId()) {
		case R.id.menu_quit:
			try {
				finish();
			} catch (Throwable e) {
				Log.e(getString(R.string.app_name), e.getMessage());
			}
			return false;
		case R.id.menu_about:
			final AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			
			alertDialog.setTitle("About " + getString(R.string.app_name));
			alertDialog.setMessage(getString(R.string.app_name) + " " +
					getString(R.string.app_version) +
					"\nFull source code available at http://sf.net/project/xmp");
			alertDialog.setButton("Cool", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					alertDialog.dismiss();
				}
			});
			alertDialog.show();			
		}
		return false;
	}
}
