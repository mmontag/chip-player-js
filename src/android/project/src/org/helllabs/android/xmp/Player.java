package org.helllabs.android.xmp;

import android.app.ListActivity;
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
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.helllabs.android.xmp.R;

public class Player extends ListActivity {
	static final int SETTINGS_REQUEST = 45;
	String media_path;
	Xmp xmp = new Xmp();	/* used to get mod info */
	ModPlayer player;		/* actual mod player */ 
	ImageButton playButton, stopButton, backButton, forwardButton;
	SeekBar seekBar;
	Thread progressThread;
	boolean playing = false;
	boolean seeking = false;
	boolean shuffleMode = true;
	boolean paused = false;
	boolean isBadDir = false;
	TextView infoName, infoType, infoLen, infoTime;
	TextView infoNpat, infoChn, infoIns, infoSmp;
	TextView infoTpo, infoBpm, infoPos, infoPat; 
	TextView infoInsList;
	int playIndex;
	RandomIndex ridx;
	SharedPreferences settings;
	LinearLayout infoMeterLayout;
	Meter infoMeter;
	LinearLayout infoLayout;
	BitmapDrawable image;
	final Handler handler = new Handler();
	
    final Runnable endSongRunnable = new Runnable() {
        public void run() {
        	/*
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
    		*/
        }
    };
    
    final Runnable updateInfoRunnable = new Runnable() {
    	private int oldTpo = -1;
    	private int oldBpm = -1;
    	private int oldPos = -1;
    	private int oldPat = -1;
    	private int[] volumes = new int[32];
    	private int count = 0;
    	
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

        	if (++count > 10) {
        		int meterType = Integer.parseInt(settings.getString(Settings.PREF_METERS, "2"));
        		if (infoMeter.getType() != meterType)
        			infoMeter = createMeter(meterType, infoMeter.getChannels());
        		count = 0;
        	}

        	player.getVolumes(volumes);
        	infoMeter.setVolumes(volumes);
        }
    };
    
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
    		int t = 0;
    		
    		do {
    			t = player.time();
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
    
	private Meter createMeter(int type, int size) {
       	Meter meter;
		
       	switch (type) {
       	case 1:
       		meter = new LedMeter(infoMeterLayout, size);
       		break;
       	case 2:
       		meter = new BarMeter(infoMeterLayout, size);
       		break;
       	default:
       		meter = new EmptyMeter(infoMeterLayout, size);
       		break;       		
       	}
       	
       	return meter;
	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.player);
		
		settings = PreferenceManager.getDefaultSharedPreferences(this);

		player = ModPlayer.getInstance(this);
		
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
		
		/* Set background here because we want to keep aspect ratio */
		image = new BitmapDrawable(BitmapFactory.decodeResource(getResources(),
												R.drawable.logo));
		image.setGravity(Gravity.CENTER);
		infoLayout.setBackgroundDrawable(image.getCurrent());
		
		playButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				
				//Debug.startMethodTracing("xmp");
				
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
								
				playIndex = 0;
				unpause();
				playNewMod(0);
		    }
		});
		
		stopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				stopPlayingMod();
				//Debug.stopMethodTracing();
		    }
		});
		
		backButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				player.stop();
				unpause();
		    }
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				player.stop();
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
				
				if (!playing) {
					seekBar.setProgress(0);
					return;
				}
				
				player.seek(s.getProgress());
				seeking = false;
			}
		});
	}
	
	@Override
	public void onDestroy() {
		player.stop();
		super.onDestroy();
	}
	
	void playNewMod(int position) {
		/* Sanity check */
		/*
		if (position < 0 || position >= modList.size())
			position = 0;
		*/
						
		ModInfo m = xmp.getModInfo(media_path + "/" + "bla");
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
       	
       	int meterType = Integer.parseInt(settings.getString(Settings.PREF_METERS, "2"));
       	infoMeter = createMeter(meterType, m.chn);
       	       		
       	player.play(m.filename);
       	
       	/* Show list of instruments */
       	StringBuffer insList = new StringBuffer();
       	String[] instrument = player.getInstruments();
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
		if (!playing)
			return;
		
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

		unpause();
		playNewMod(position);
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
}
