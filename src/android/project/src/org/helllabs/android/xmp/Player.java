package org.helllabs.android.xmp;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.BitmapFactory;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;
import android.widget.ViewFlipper;

public class Player extends Activity {
	static final int SETTINGS_REQUEST = 45;
	String media_path;
	ModInterface modPlayer;	/* actual mod player */
	ImageButton playButton, stopButton, backButton, forwardButton;
	ImageButton loopButton;
	SeekBar seekBar;
	Thread progressThread;
	boolean seeking = false;
	boolean shuffleMode = true;
	boolean loopListMode = false;
	boolean paused = false;
	boolean finishing = false;
	boolean showTime, showElapsed;
	TextView[] infoName = new TextView[2];
	TextView[] infoType = new TextView[2];
	TextView infoLen, infoTime;
	TextView infoNpat, infoChn, infoIns, infoSmp;
	TextView infoTpo, infoBpm, infoPos, infoPat; 
	TextView infoInsList, elapsedTime;
	ViewFlipper flipper;
	int flipperPage;
	String[] fileArray = null;
	SharedPreferences prefs;
	LinearLayout infoMeterLayout;
	Meter infoMeter;
	LinearLayout infoLayout;
	BitmapDrawable image;
	final Handler handler = new Handler();
	int latency;
	int totalTime;
	String fileName, insList;
	boolean endPlay = false;

	private ServiceConnection connection = new ServiceConnection() {
		public void onServiceConnected(ComponentName className, IBinder service) {
			modPlayer = ModInterface.Stub.asInterface(service);
	       	flipperPage = 0;
			
			try {
				modPlayer.registerCallback(playerCallback);
			} catch (RemoteException e) { }
			
			if (fileArray != null && fileArray.length > 0) {
				// Start new queue
				playNewMod(fileArray);
			} else {
				// Reconnect to existing service
				try {
					showNewMod(modPlayer.getFileName(), modPlayer.getInstruments());
						
					if (modPlayer.isPaused()) {
						pause();
					} else {
						unpause();
					}
				} catch (RemoteException e) { }
			}
		}

		public void onServiceDisconnected(ComponentName className) {
			modPlayer = null;
		}
	};
	
    private PlayerCallback playerCallback = new PlayerCallback.Stub() {
        public void newModCallback(String name, String[] instruments) {
        	Log.i("Xmp Player", "Show module data");
            showNewMod(name, instruments);
        }
        
        public void endPlayCallback() {
        	Log.i("Xmp Player", "End progress thread");
			endPlay = true;
			if (progressThread != null && progressThread.isAlive()) {
				try {
					progressThread.join();
				} catch (InterruptedException e) { }
			}
			finish();
        }
    };
    
    final Runnable updateInfoRunnable = new Runnable() {
    	int[] tpo = new int[10];
    	int[] bpm = new int[10];
    	int[] pos = new int[10];
    	int[] pat = new int[10];
    	int[] time = new int[10];
    	int oldTpo = -1;
    	int oldBpm = -1;
    	int oldPos = -1;
    	int oldPat = -1;
    	int oldTime = -1;
    	int[][] volumes = new int[10][32];
    	int before = 0, now;
    	boolean oldShowElapsed;
    	
        public void run() {
        	now = (before + latency) % 10;
        	
        	try {
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
	
	        	if (showTime) {
		        	time[now] = modPlayer.time() / 10;
		        	if (time[before] != oldTime || showElapsed != oldShowElapsed) {
			        	int t = time[before];
			        	if (t < 0)
			        		t = 0;
			        	if (showElapsed) {
			        		elapsedTime.setText(String.format("%d:%02d", t / 60, t % 60));
			        	} else {
			        		t = totalTime - t;
			        		elapsedTime.setText(String.format("-%d:%02d", t / 60, t % 60));
			        	}
		        		oldTime = time[before];
		        		oldShowElapsed = showElapsed;
		        	}
	        	}
			} catch (RemoteException e) { }
        }
    };
    
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
    		int t = 0;
    		
    		do {
    			try {
					t = modPlayer.time();
				} catch (RemoteException e) {

				}
    			//Log.v(getString(R.string.app_name), "t = " + t);
    			if (t >= 0) {
    				if (!seeking && !paused)
    					seekBar.setProgress(t);
    			}
    			
    			try {
					sleep(100);
				} catch (InterruptedException e) { }
				
				handler.post(updateInfoRunnable);
    		} while (t >= 0 && !endPlay);
    		
    		seekBar.setProgress(0);
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
	protected void onNewIntent(Intent intent) {
		boolean reconnect = false;
		
		Log.i("Xmp Player", "Start player interface");
		
		String path = null;
		if (intent.getData() != null) {
			path = intent.getData().getPath();
		}

		fileArray = null;
		
		if (path != null) {		// from intent filter
			fileArray = new String[1];
			fileArray[0] = path;
			shuffleMode = false;
			loopListMode = false;
		} else {	
			Bundle extras = intent.getExtras();
			if (extras != null) {
				fileArray = extras.getStringArray("files");	
				shuffleMode = extras.getBoolean("shuffle");
				loopListMode = extras.getBoolean("loop");
			} else {
				reconnect = true;
			}
		}
		
    	Intent service = new Intent(this, ModService.class);
    	if (!reconnect) {
    		Log.i("Xmp Player", "Start service");
    		startService(service);
    	}
    	if (!bindService(service, connection, 0)) {
    		Log.e("Xmp Player", "Can't bind to service");
    		finish();
    	}
	}
	
    void setFont(TextView name, String path, int res) {
        Typeface font = Typeface.createFromAsset(this.getAssets(), path); 
        name.setTypeface(font); 
    }
    
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.player);
		
		Log.i("Xmp Player", "Create player interface");
		
		prefs = PreferenceManager.getDefaultSharedPreferences(this);
		
		showTime = prefs.getBoolean(Settings.PREF_SHOW_TIME, false);
		showElapsed = true;
		
		latency = prefs.getInt(Settings.PREF_BUFFER_MS, 500) / 100;
		if (latency > 9)
			latency = 9;
		
		onNewIntent(getIntent());
    	
		infoName[0] = (TextView)findViewById(R.id.info_name_0);
		infoType[0] = (TextView)findViewById(R.id.info_type_0);
		infoName[1] = (TextView)findViewById(R.id.info_name_1);
		infoType[1] = (TextView)findViewById(R.id.info_type_1);
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
		elapsedTime = (TextView)findViewById(R.id.elapsed_time);
		flipper = (ViewFlipper)findViewById(R.id.info_flipper);
		
		flipper.setInAnimation(this, R.anim.slide_in_right);
		flipper.setOutAnimation(this, R.anim.slide_out_left);

        Typeface font = Typeface.createFromAsset(this.getAssets(), "fonts/Michroma.ttf");
        for (int i = 0; i < 2; i++) {
        	infoName[i].setTypeface(font);
        	infoName[i].setIncludeFontPadding(false);
        	infoType[i].setTypeface(font);
        	infoType[i].setTextSize(TypedValue.COMPLEX_UNIT_DIP, 12);
        }
        
        TextView instruments = (TextView)findViewById(R.id.text_instruments);
        instruments.setTypeface(font);
        instruments.setTextSize(TypedValue.COMPLEX_UNIT_DIP, 16);
        instruments.setLineSpacing(0, 1.2f);
		
		if (!showTime)
			elapsedTime.setVisibility(LinearLayout.GONE);
		
		playButton = (ImageButton)findViewById(R.id.play);
		stopButton = (ImageButton)findViewById(R.id.stop);
		backButton = (ImageButton)findViewById(R.id.back);
		forwardButton = (ImageButton)findViewById(R.id.forward);
		loopButton = (ImageButton)findViewById(R.id.loop);
		
		// Set background here because we want to keep aspect ratio
		image = new BitmapDrawable(BitmapFactory.decodeResource(getResources(),
												R.drawable.logo));
		image.setGravity(Gravity.CENTER);
		image.setAlpha(48);
		infoLayout.setBackgroundDrawable(image.getCurrent());
		loopButton.setImageResource(R.drawable.loop_off);
		
		loopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				try {
					if (modPlayer.toggleLoop()) {
						loopButton.setImageResource(R.drawable.loop_on);
					} else {
						loopButton.setImageResource(R.drawable.loop_off);
					}
				} catch (RemoteException e) {

				}
			}
		});
		
		playButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				//Debug.startMethodTracing("xmp");				
				if (modPlayer == null)
					return;
				
				synchronized (this) {
					try {
						modPlayer.pause();
					} catch (RemoteException e) {

					}
					
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
				if (modPlayer == null)
					return;
				
				stopPlayingMod();
				finish();
		    }
		});
		
		backButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				if (modPlayer == null)
					return;
				
				try {
					if (modPlayer.time() > 20) {
						modPlayer.seek(0);
					} else {
						modPlayer.prevSong();
					}
					unpause();
				} catch (RemoteException e) {

				}
			}
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {				
				if (modPlayer == null)
					return;
				
				try {
					modPlayer.nextSong();
				} catch (RemoteException e) {

				}
				unpause();
		    }
		});
		
		elapsedTime.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				showElapsed ^= true;
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
				if (modPlayer != null) {
					try {
						modPlayer.seek(s.getProgress());
					} catch (RemoteException e) { }
				}
				seeking = false;
			}
		});
	}
	
	@Override
	public void onDestroy() {
		super.onDestroy();
		if (modPlayer != null) {
			try {
				modPlayer.unregisterCallback(playerCallback);
			} catch (RemoteException e) { }
		}
		Log.i("Xmp Player", "Unbind service");
		unbindService(connection);
	}

	void showNewMod(String fileName, String[] instruments) {
		this.fileName = fileName;
		
     	StringBuffer insList = new StringBuffer();
       	if (instruments.length > 0)
       		insList.append(instruments[0]);
       	for (int i = 1; i < instruments.length; i++) {
       		insList.append('\n');
       		insList.append(instruments[i]);
       	}
       	
       	this.insList = insList.toString();
       	
		handler.post(showNewModRunnable);
	}
	
	final Runnable showNewModRunnable = new Runnable() {
		public void run() {
			ModInfo m = InfoCache.getModInfo(fileName);
			totalTime = m.time / 1000;
	       	seekBar.setProgress(0);
	       	seekBar.setMax(m.time / 100);
	        
	       	flipperPage = (flipperPage + 1) % 2;
	       	infoName[flipperPage].setText(m.name);
	       	infoType[flipperPage].setText(m.type);
	       	flipper.showNext();
	       	
	       	infoLen.setText(Integer.toString(m.len));
	       	infoNpat.setText(Integer.toString(m.pat));
	       	infoChn.setText(Integer.toString(m.chn));
	       	infoIns.setText(Integer.toString(m.ins));
	       	infoSmp.setText(Integer.toString(m.smp));
	       	infoTime.setText(Integer.toString((m.time + 500) / 60000) + "min" + 
	       			Integer.toString(((m.time + 500) / 1000) % 60) + "s");
	       	infoInsList.setText(insList);
	       	
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
	       	
	        progressThread = new ProgressThread();
	        progressThread.start();
		}
	};
	
	void playNewMod(String[] files) {      	 
       	try {
			modPlayer.play(files, shuffleMode, loopListMode);
		} catch (RemoteException e) { }
	}
	
	void stopPlayingMod() {
		if (finishing)
			return;
		
		finishing = true;
		try {
			modPlayer.stop();
		} catch (RemoteException e1) { }
		paused = false;

		if (progressThread != null && progressThread.isAlive()) {
			try {
				progressThread.join();
			} catch (InterruptedException e) { }
		}
	}
}
