package org.helllabs.android.xmp;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
//import android.os.Debug;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.helllabs.android.xmp.R;

public class Player extends Activity {
	static final int SETTINGS_REQUEST = 45;
	String media_path;
	Xmp xmp = new Xmp();	/* used to get mod info */
	ModPlayer modPlayer;	/* actual mod player */ 
	ImageButton playButton, stopButton, backButton, forwardButton;
	SeekBar seekBar;
	Thread progressThread;
	boolean seeking = false;
	boolean single = true;
	boolean shuffleMode = true;
	boolean loopListMode = false;
	boolean paused = false;
	boolean isBadDir = false;
	boolean finishing = false;
	TextView infoName, infoType, infoLen, infoTime;
	TextView infoNpat, infoChn, infoIns, infoSmp;
	TextView infoTpo, infoBpm, infoPos, infoPat; 
	TextView infoInsList;
	String[] fileArray;
	int playIndex;
	RandomIndex ridx;
	SharedPreferences prefs;
	LinearLayout infoMeterLayout;
	Meter infoMeter;
	LinearLayout infoLayout;
	BitmapDrawable image;
	final Handler handler = new Handler();
	int latency;
	
    final Runnable endSongRunnable = new Runnable() {
        public void run() {
        	int idx;
        	
        	if (finishing)
        		return;

        	if (single) {
        		finish();
        		return;
        	}
        	
        	if (++playIndex < fileArray.length) {
        		idx = shuffleMode ? ridx.getIndex(playIndex) : playIndex;
        		playNewMod(fileArray[idx]);
        	} else {
        		if (loopListMode) {
        			playIndex = 0;
        			ridx.randomize();
        			idx = shuffleMode ? ridx.getIndex(playIndex) : playIndex;
        			playNewMod(fileArray[idx]);
        		} else {
        			finish();
        		}
        	}
        }
    };
    
    final Runnable updateInfoRunnable = new Runnable() {
    	int[] tpo = new int[10];
    	int[] bpm = new int[10];
    	int[] pos = new int[10];
    	int[] pat = new int[10];
    	int oldTpo = -1;
    	int oldBpm = -1;
    	int oldPos = -1;
    	int oldPat = -1;
    	int[][] volumes = new int[10][32];
    	int before = 0, now;
    	
        public void run() {
        	now = (before + latency) % 10;
        	
        	tpo[now] = modPlayer.getPlayTempo();
        	if (tpo[before] != oldTpo) {
        		infoTpo.setText(Integer.toString(tpo[before]));
        		oldTpo = tpo[before];
        	}

        	bpm[now] = modPlayer.getPlayBpm();
        	if (bpm[before] != oldBpm) {
        		infoBpm.setText(Integer.toString(bpm[before]));
        		oldBpm = bpm[before];
        	}

        	pos[now] = modPlayer.getPlayPos();
        	if (pos[before] != oldPos) {
        		infoPos.setText(Integer.toString(pos[before]));
        		oldPos = pos[before];
        	}

        	pat[now] = modPlayer.getPlayPat();
        	if (pat[before] != oldPat) {
        		infoPat.setText(Integer.toString(pat[before]));
        		oldPat = pat[before];
        	}

        	modPlayer.getVolumes(volumes[now]);
        	infoMeter.setVolumes(volumes[before]);
        	before++;
        	if (before >= 10)
        		before = 0;
        }
    };
    
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
    		int t = 0;
    		
    		do {
    			t = modPlayer.time();
    			//Log.v(getString(R.string.app_name), "t = " + t);
    			if (t >= 0) {
    				if (!seeking && !paused)
    					seekBar.setProgress(t);
    			}
    			
    			try {
					sleep(100);
				} catch (InterruptedException e) {
					Log.e(getString(R.string.app_name), e.getMessage());
				}
				
				handler.post(updateInfoRunnable);
    		} while (t >= 0);
    		
    		seekBar.setProgress(0);
    		handler.post(endSongRunnable);
    	}
    };

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
		setContentView(R.layout.player);
		
		prefs = PreferenceManager.getDefaultSharedPreferences(this);
		
		String path = null;
		if (getIntent().getData() != null) {
			path = getIntent().getData().getEncodedPath();
		}
		
		if (path != null) {
			xmp.initContext();
			fileArray = new String[1];
			fileArray[0] = path;
			single = true;
			shuffleMode = false;
			loopListMode = false;
		} else {
			Bundle extras = getIntent().getExtras();
			if (extras == null)
				return;
		
			fileArray = extras.getStringArray("files");
			single = extras.getBoolean("single");	
			shuffleMode = extras.getBoolean("shuffle");
			loopListMode = extras.getBoolean("loop");
		}

    	latency = prefs.getInt(Settings.PREF_BUFFER_MS, 500) / 100;
    	if (latency > 9)
    		latency = 9;
    	
		ridx = new RandomIndex(fileArray.length);
		modPlayer = new ModPlayer(this);
		
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
		infoMeterLayout = (LinearLayout)findViewById(R.id.info_meters);
		infoInsList = (TextView)findViewById(R.id.info_ins_list);
		infoLayout = (LinearLayout)findViewById(R.id.info_layout);
		
		playButton = (ImageButton)findViewById(R.id.play);
		stopButton = (ImageButton)findViewById(R.id.stop);
		backButton = (ImageButton)findViewById(R.id.back);
		forwardButton = (ImageButton)findViewById(R.id.forward);
		
		// Set background here because we want to keep aspect ratio
		image = new BitmapDrawable(BitmapFactory.decodeResource(getResources(),
												R.drawable.logo));
		image.setGravity(Gravity.CENTER);
		infoLayout.setBackgroundDrawable(image.getCurrent());
		
		playButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				
				//Debug.startMethodTracing("xmp");
				
				synchronized (this) {
					modPlayer.pause();
					
					if (paused) {
						unpause();
					} else {
						pause();
					}
				}
		    }
		});
		
		stopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				//Debug.stopMethodTracing();
				stopPlayingMod();
				finish();
		    }
		});
		
		backButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				playIndex -= 2;
				if (playIndex < -1)
					playIndex += fileArray.length;
				modPlayer.stop();
				unpause();
		    }
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				modPlayer.stop();
				unpause();
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
				modPlayer.seek(s.getProgress());
				seeking = false;
			}
		});

		if (fileArray.length > 0) {
			playIndex = 0;
			int idx = shuffleMode ? ridx.getIndex(playIndex) : playIndex;
			playNewMod(fileArray[idx]);
		}
	}
	
	@Override
	public void onDestroy() {
		stopPlayingMod();
		modPlayer.end();
		super.onDestroy();
	}

	void playNewMod(String fileName) {
		ModInfo m = xmp.getModInfo(fileName);
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
       	
       	int meterType = Integer.parseInt(prefs.getString(Settings.PREF_METERS, "2"));
       	switch (meterType) {
       	case 1:
       		infoMeter = new LedMeter(infoMeterLayout, m.chn);
       		break;
       	case 2:
       		infoMeter = new BarMeter(infoMeterLayout, m.chn);
       		break;
       	default:
       		infoMeter = new EmptyMeter(infoMeterLayout, m.chn);
       		break;       		
       	}
 
       	modPlayer.play(m.filename);
       	
       	/* Show list of instruments */
       	StringBuffer insList = new StringBuffer();
       	String[] instrument = modPlayer.getInstruments();
       	if (instrument.length > 0)
       		insList.append(instrument[0]);
       	for (int i = 1; i < instrument.length; i++) {
       		insList.append('\n');
       		insList.append(instrument[i]);
       	}
       	infoInsList.setText(insList.toString());

        progressThread = new ProgressThread();
        progressThread.start();	
	}
	
	void stopPlayingMod() {
		if (finishing)
			return;
		
		finishing = true;
		modPlayer.stop();
		paused = false;
		
		try {
			progressThread.join();
		} catch (InterruptedException e) {
			Log.e(getString(R.string.app_name), e.getMessage());
		}
	}
	
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
    	if(event.getAction() == KeyEvent.ACTION_DOWN) {
    		switch(keyCode) {
    		case KeyEvent.KEYCODE_BACK:
    			stopPlayingMod();
    			finish();
    			return true;
    		}
    	}
    	
    	return super.onKeyDown(keyCode, event);
    }
}
