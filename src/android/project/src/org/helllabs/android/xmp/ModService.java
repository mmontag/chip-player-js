package org.helllabs.android.xmp;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.IBinder;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.util.Log;


public class ModService extends Service {
	Xmp xmp = new Xmp();
	AudioTrack audio;
	boolean paused;
	Thread playThread;
	SharedPreferences prefs;
	int minSize;
	boolean stereo;
	boolean interpolate;
    private NotificationManager nm;
    private static final int NOTIFY_ID = R.layout.player;
    int playIndex;
    RandomIndex ridx;
    String[] fileArray;
    final RemoteCallbackList<PlayerCallback> callbacks =
		new RemoteCallbackList<PlayerCallback>();
       
    @Override
	public void onCreate() {
    	super.onCreate();
    	
    	nm = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
    	PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
    					new Intent(this, Player.class), 0);
        nm.cancel(NOTIFY_ID);
        Notification notification = new Notification(
        		R.drawable.notification, null, System.currentTimeMillis());
        notification.setLatestEventInfo(this, getText(R.string.app_name),
        	      "bla", contentIntent);
        nm.notify(NOTIFY_ID, notification);
    	
   		prefs = PreferenceManager.getDefaultSharedPreferences(this);
   		
   		int sampleRate = Integer.parseInt(prefs.getString(Settings.PREF_SAMPLING_RATE, "44100"));
   		int bufferMs = prefs.getInt(Settings.PREF_BUFFER_MS, 500);
   		stereo = prefs.getBoolean(Settings.PREF_STEREO, true);
   		interpolate = prefs.getBoolean(Settings.PREF_INTERPOLATION, true);
   		
   		int bufferSize = (sampleRate * (stereo ? 2 : 1) * 2 * bufferMs / 1000) & ~0x3;
	
   		int channelConfig = stereo ?
   				AudioFormat.CHANNEL_CONFIGURATION_STEREO :
   				AudioFormat.CHANNEL_CONFIGURATION_MONO;
   		
		minSize = AudioTrack.getMinBufferSize(
				sampleRate,
				channelConfig,
				AudioFormat.ENCODING_PCM_16BIT);

		audio = new AudioTrack(
				AudioManager.STREAM_MUSIC, sampleRate,
				channelConfig,
				AudioFormat.ENCODING_PCM_16BIT,
				minSize < bufferSize ? bufferSize : minSize,
				AudioTrack.MODE_STREAM);

		xmp.initContext();
		xmp.init(sampleRate);
		paused = false;
    }

    @Override
	public void onDestroy() {
    	end();
    	nm.cancel(NOTIFY_ID);
    }

	@Override
	public IBinder onBind(Intent intent) {
		return binder;
	}
	
	private class PlayRunnable implements Runnable {
    	public void run() {
    		short buffer[] = new short[minSize];
    		
       		while (xmp.playFrame() == 0) {
       			int size = xmp.softmixer();
       			buffer = xmp.getBuffer(size, buffer);
       			audio.write(buffer, 0, size / 2);
       			
       			while (paused) {
       				try {
						Thread.sleep(500);
					} catch (InterruptedException e) {
						break;
					}
       			}
       		}
       		
       		audio.stop();
       		xmp.endPlayer();
       		xmp.releaseModule();
       		callbacks.finishBroadcast();
       		       	
        	if (++playIndex < fileArray.length)
        		playMod(playIndex);
    	}
    }

	protected void end() {
    	xmp.stopModule();
    	paused = false;
    	try {
			playThread.join();
		} catch (InterruptedException e) { }
    	xmp.deinit();
    	audio.release();
    	callbacks.finishBroadcast();
    }
	
    public void playMod(int index) {
    	//int idx = shuffleMode ? ridx.getIndex(index) : index;
    	int idx = index;

		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));

   		if (xmp.loadModule(fileArray[idx]) < 0) {
   			return;
   		}
   		    	
    	final int numClients = callbacks.beginBroadcast();
    	for (int i = 0; i < numClients; i++) {
    		try {
				callbacks.getBroadcastItem(i).newModCallback(
							fileArray[idx],	xmp.getInstruments());
			} catch (RemoteException e) { }
    	}
   		
		String volBoost = prefs.getString(Settings.PREF_VOL_BOOST, "1");
		xmp.optAmplify(Integer.parseInt(volBoost));
		xmp.optMix(prefs.getInt(Settings.PREF_PAN_SEPARATION, 70));
		xmp.optStereo(prefs.getBoolean(Settings.PREF_STEREO, true));
		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));	   		

   		audio.play();
   		xmp.startPlayer();
   		
   		PlayRunnable playRunnable = new PlayRunnable();
   		playThread = new Thread(playRunnable);
   		playThread.start();
    }

	private final ModInterface.Stub binder = new ModInterface.Stub() {
		public void play(String[] files) {
			fileArray = files;
			playMod(0);
		}
	    
	    public void stop() {
	    	xmp.stopModule();
	    	paused = false;
	    }
	    
	    public void pause() {
	    	paused = !paused;
	    }
	    
	    public int time() {
	    	return xmp.time();
	    }
	
		public void seek(int seconds) {
			xmp.seek(seconds);
		}
		
		public int getPlayTempo() {
			return xmp.getPlayTempo();
		}
		
		public int getPlayBpm() {
			return xmp.getPlayBpm();
		}
		
		public int getPlayPos() {
			return xmp.getPlayPos();
		}
		
		public int getPlayPat() {
			return xmp.getPlayPat();
		}
		
		public void getVolumes(int[] volumes) {
			xmp.getVolumes(volumes);
		}
		
		public void nextSong() {
			
		}
		
		public void prevSong() {
			
		}

		public boolean toggleLoop() throws RemoteException {
			if (xmp.testFlag(Xmp.XMP_CTL_LOOP)) {
				xmp.resetFlag(Xmp.XMP_CTL_LOOP);
				return false;
			} else {
				xmp.setFlag(Xmp.XMP_CTL_LOOP);
				return true;
			}
		}
		
		public void registerCallback(PlayerCallback cb) {
        	if (cb != null)
            	callbacks.register(cb);
        }
        
        public void unregisterCallback(PlayerCallback cb) {
            if (cb != null)
            	callbacks.unregister(cb);
        }
	};
}
