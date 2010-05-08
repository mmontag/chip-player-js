package org.helllabs.android.xmp;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.List;
import android.app.ListActivity;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
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
	
	static final String MEDIA_PATH = new String("/sdcard/");
	private List<ModInfo> modlist = new ArrayList<ModInfo>();
	private Xmp xmp = new Xmp();
    private Interface modInterface;
	private Button playButton, stopButton, backButton, forwardButton;
	
	class ModFilter implements FilenameFilter {
	    public boolean accept(File dir, String name) {
	    	Log.v(getString(R.string.app_name), "** " + dir + "/" + name);
	        return (xmp.testModule(dir + "/" + name) == 0);
	    }
	}
	
	@Override
	public void onCreate(Bundle icicle) {
		xmp.init();
		
		playButton = (Button)findViewById(R.id.play);
		stopButton = (Button)findViewById(R.id.stop);
		backButton = (Button)findViewById(R.id.back);
		forwardButton = (Button)findViewById(R.id.forward);
		
		/*playButton.setOnClickListener(new OnClickListener() {
			@Override
		    public void onClick(View v) {
		    	finish();
		    }
		}); */
		
		try {
			super.onCreate(icicle);
			setContentView(R.layout.playlist);
			updatePlaylist();
		} catch (NullPointerException e) {
			Log.v(getString(R.string.app_name), e.getMessage());
		}
	}

	public void updatePlaylist() {
		File home = new File(MEDIA_PATH);
		for (File file : home.listFiles(new ModFilter())) {
			ModInfo m = xmp.getModInfo(MEDIA_PATH + "/" + file.getName());
			/* m.title = file.getName();
			m.filename = file.getName(); */
			modlist.add(m);
		}
		
        ModInfoAdapter playlist = new ModInfoAdapter(this,
        			R.layout.song_item, R.id.info, modlist);
        setListAdapter(playlist);
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
        try {
            modInterface.playFile(position);
        } catch (DeadObjectException e) {
            Log.e(getString(R.string.app_name), e.getMessage());
        } catch (RemoteException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
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
