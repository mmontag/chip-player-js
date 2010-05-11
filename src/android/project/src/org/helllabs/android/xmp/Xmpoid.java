package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.List;
import android.app.ListActivity;
import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
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
                              tt.setText(o.name);                            }
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
	private ModService player;		/* actual mod player */ 
	private ImageButton playButton, stopButton, backButton, forwardButton;
	private SeekBar seekBar;
	private boolean playing = false;
	private boolean seeking = false;
	private ViewFlipper flipper;
	private TextView infoName, infoType;
	
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
			playing = true;
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
    		} while (t >= 0);
    		
    		seekBar.setProgress(0);
    		playing = false;
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
		
		player = new ModService();
		
		/* Info view widgets */
		flipper = (ViewFlipper)findViewById(R.id.flipper);
		infoName = (TextView)findViewById(R.id.info_name);
		infoType = (TextView)findViewById(R.id.info_type);
		
		playButton = (ImageButton)findViewById(R.id.play);
		stopButton = (ImageButton)findViewById(R.id.stop);
		backButton = (ImageButton)findViewById(R.id.back);
		forwardButton = (ImageButton)findViewById(R.id.forward);
		
		playButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
		    	finish();
		    }
		});
		
		stopButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				player.stop();
				try {
					progressThread.join();
				} catch (InterruptedException e) {
					Log.e(getString(R.string.app_name), e.getMessage());
				}
				flipper.showPrevious();
				
		    }
		});
		
		backButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
		    	finish();
		    }
		});
		
		forwardButton.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
		    	finish();
		    }
		});
		
		seekBar = (SeekBar)findViewById(R.id.seek);
		seekBar.setProgress(0);
		
		seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onClick(View v) {
		    	finish();
		    }

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

        	seekBar.setProgress(0);
        	seekBar.setMax(modList.get(position).time / 100);
        	
        	infoName.setText(modList.get(position).name);
        	infoType.setText(modList.get(position).type);
        	flipper.showNext();
        	
            player.play(modList.get(position).filename);
            progressThread = new ProgressThread();
            progressThread.start();
            
		} catch (InterruptedException e) {
			Log.e(getString(R.string.app_name), e.getMessage());
		}		
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		playNewMod(position);
	}

}
