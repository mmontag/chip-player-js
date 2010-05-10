package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.List;
import android.app.ListActivity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.DeadObjectException;
import android.os.IBinder;
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
import android.widget.SeekBar.OnSeekBarChangeListener;

import org.helllabs.android.xmp.R;


public class ModPlayer extends ListActivity {
	
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
	private Xmp xmp = new Xmp();
    private Interface modInterface;
	private ImageButton playButton, stopButton, backButton, forwardButton;
	private SeekBar seekBar;
	boolean playing = false;
	boolean seeking = false;
	
	private class ProgressThread extends Thread {
		@Override
    	public void run() {
			playing = true;
    		int t = 0;
    		do {
    			try {
    				t = modInterface.time();
    			} catch (RemoteException e) {
    				Log.e(getString(R.string.app_name), e.getMessage());
    			}
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
		    	try {
					modInterface.stop();
				} catch (RemoteException e) {
					Log.e(getString(R.string.app_name), e.getMessage());
				}
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
				seeking = false;
				
				if (!playing)
					return;
				
				try {
					modInterface.seekPosition(s.getProgress());
				} catch (RemoteException e) {
					Log.e(getString(R.string.app_name), e.getMessage());
				}
				
			}
		});
			
		this.bindService(new Intent(ModPlayer.this, ModService.class),
					mConnection, Context.BIND_AUTO_CREATE);
	}

	public void updatePlaylist() {
		File home = new File(MEDIA_PATH);
		for (File file : home.listFiles(new ModFilter())) {
			ModInfo m = xmp.getModInfo(MEDIA_PATH + file.getName());
			
			try {
				modInterface.addSongPlaylist(MEDIA_PATH + file.getName());
			} catch (RemoteException e) {
				Log.v(getString(R.string.app_name), e.getMessage());
			}
			modList.add(m);
		}
		
        ModInfoAdapter playlist = new ModInfoAdapter(this,
        			R.layout.song_item, R.id.info, modList);
        setListAdapter(playlist);
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		try {
			if (playing) {
				modInterface.stop();
				progressThread.join();
			}
		      
        	seekBar.setProgress(0);
        	seekBar.setMax(modList.get(position).time / 100);
            modInterface.playFile(position);
            progressThread = new ProgressThread();
            progressThread.start();
        } catch (DeadObjectException e) {
            Log.e(getString(R.string.app_name), e.getMessage());
        } catch (RemoteException e) {
        	Log.e(getString(R.string.app_name), e.getMessage());
		} catch (InterruptedException e) {
			Log.e(getString(R.string.app_name), e.getMessage());
		}
	}
	
    private ServiceConnection mConnection = new ServiceConnection()
    {
    	public void onServiceConnected(ComponentName className, IBinder service) {
    		modInterface = Interface.Stub.asInterface((IBinder)service);
    		updatePlaylist();
    	}

    	public void onServiceDisconnected(ComponentName className) {
    		modInterface = null;
    	}
    };

}
