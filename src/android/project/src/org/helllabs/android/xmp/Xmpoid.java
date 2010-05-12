package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Random;

import android.app.ListActivity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
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
	private ViewFlipper flipper;
	private TextView infoName, infoType, infoLen;
	private TextView infoNpat, infoIns, infoSmp;
	private TextView infoTpo, infoBpm, infoPos, infoPat; 
	private int playIndex;
	private RandomIndex ridx;
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
    				if (!seeking)
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
				int idx[] = new int[modList.size()];
				for (int i = 0; i < modList.size(); i++) {
					idx[i] = i;
				}
				
				ridx = new RandomIndex(idx.length);				
				single = false;
				
				flipper.setAnimation(AnimationUtils.loadAnimation(v.getContext(), R.anim.slide_left));				
				flipper.showNext();
				playIndex = 0;
				playNewMod(0);
		    }
		});
		
		stopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				single = true;
				player.stop();
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
				}
		    }
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				if (playing && !single) {
					player.stop();
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
		File home = new File(MEDIA_PATH);
		for (File file : home.listFiles(new ModFilter())) {
			ModInfo m = xmp.getModInfo(MEDIA_PATH + file.getName());
			modList.add(m);
		}		
        ModInfoAdapter playlist = new ModInfoAdapter(this,
        			R.layout.song_item, R.id.info, modList);
        setListAdapter(playlist);
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
		playNewMod(position);
	}
	
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.options_menu, menu);
	    return true;
	}
}
