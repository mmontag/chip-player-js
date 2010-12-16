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

import org.helllabs.android.xmp.Watchdog.onTimeoutListener;


public class ModService extends Service {
	final Xmp xmp = new Xmp();
	AudioTrack audio;
	Thread playThread;
	SharedPreferences prefs;
	Watchdog watchdog;
	int minSize;
	boolean stereo;
	boolean interpolate;
    private NotificationManager nm;
    private static final int NOTIFY_ID = R.layout.player;
    RandomIndex ridx;
	boolean shuffleMode = true;
	boolean loopListMode = false;
	boolean stopPlaying = false;
	boolean isPlaying;
	boolean restartList;
	boolean returnToPrev;
	boolean paused;
	String fileName;			// currently playing file
    String[] fileArray = null;
    final RemoteCallbackList<PlayerCallback> callbacks =
		new RemoteCallbackList<PlayerCallback>();
    
    @Override
	public void onCreate() {
    	super.onCreate();
    	
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

		isPlaying = false;
		paused = false;
		
		watchdog = new Watchdog(15);
 		watchdog.setOnTimeoutListener(new onTimeoutListener() {
			public void onTimeout() {
				Log.e("Xmp ModService", "Stopped by watchdog");
		    	stopSelf();
			}
		});
 		watchdog.start();
    }

    @Override
	public void onDestroy() {
    	watchdog.stop();
    	nm.cancel(NOTIFY_ID);
    	end();
    }

	@Override
	public IBinder onBind(Intent intent) {
		return binder;
	}
	
	private class PlayRunnable implements Runnable {
    	public void run() {
    		do {
	    		for (int index = 0; index < fileArray.length; index++) {
		        	int idx = shuffleMode ? ridx.getIndex(index) : index;
		        	
		    		Log.i("Xmp ModService", "Load " + fileArray[idx]);
		       		if (xmp.loadModule(fileArray[idx]) < 0)
		       			continue;

		       		fileName = fileArray[idx];
		       		String title = fileArray.length > 1 ?
		       			String.format("%s (%d/%d)",	xmp.getTitle(), index + 1, fileArray.length) :
		       			xmp.getTitle(); 
		       		createNotification(title);
		       		
		    		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
		    		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));
			       		    	
		        	final int numClients = callbacks.beginBroadcast();
		        	for (int i = 0; i < numClients; i++) {
		        		try {
		    				callbacks.getBroadcastItem(i).newModCallback(
		    							fileName, xmp.getInstruments());
		    			} catch (RemoteException e) { }
		        	}
		        	callbacks.finishBroadcast();
		       		
		    		String volBoost = prefs.getString(Settings.PREF_VOL_BOOST, "1");
		    		xmp.optAmplify(Integer.parseInt(volBoost));
		    		xmp.optMix(prefs.getInt(Settings.PREF_PAN_SEPARATION, 70));
		    		xmp.optStereo(prefs.getBoolean(Settings.PREF_STEREO, true));
		    		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
		    		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));	   		
		
		       		audio.play();
		       		xmp.startPlayer();
		    			    		
		    		short buffer[] = new short[minSize];
		    		
		       		while (xmp.playFrame() == 0) {
		       			int size = xmp.softmixer();
		       			buffer = xmp.getBuffer(size, buffer);
		       			audio.write(buffer, 0, size / 2);
		       			
		       			while (paused) {
		       				watchdog.refresh();
		       				try {
								Thread.sleep(500);
							} catch (InterruptedException e) {
								break;
							}
		       			}
		       			
		       			watchdog.refresh();
		       		}
		      		
		       		audio.stop();
		       		xmp.endPlayer();
		       		xmp.releaseModule();
		       		
		       		if (restartList) {
		       			index = -1;
		       			restartList = false;
		       			continue;
		       		}
		       		
		       		if (returnToPrev) {
		       			index -= 2;
		       			if (index < -1) {
		       				if (loopListMode)
		       					index += fileArray.length;
		       				else
		       					index = -1;
		       			}
		       			returnToPrev = false;
		       		}
		       		
		       		if (stopPlaying) {
		       			loopListMode = false;
		       			break;
		       		}
	    		}
	    		if (loopListMode)
	    			ridx.randomize();
    		} while (loopListMode);
       		
        	nm.cancel(NOTIFY_ID);
        	end();
        	stopSelf();
    	}
    }

	protected void end() {    	
		Log.i("Xmp ModService", "End service");
	    final int numClients = callbacks.beginBroadcast();
	    for (int i = 0; i < numClients; i++) {
	    	try {
				callbacks.getBroadcastItem(i).endPlayCallback();
			} catch (RemoteException e) { }
	    }	    
	    callbacks.finishBroadcast();
	    
	    isPlaying = false;
    	xmp.stopModule();
    	paused = false;
    	try {
			playThread.join();
		} catch (InterruptedException e) { }
    	xmp.deinit();
    	audio.release();
    }

    private void createNotification(String message) {
    	nm = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
    	if (message == null)
    		nm.cancel(NOTIFY_ID);
    	PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
    					new Intent(this, Player.class), 0);
        Notification notification = new Notification(
        		R.drawable.notification, message, System.currentTimeMillis());   		
        notification.setLatestEventInfo(this, getText(R.string.app_name),
      	      message, contentIntent);
        notification.flags |= Notification.FLAG_ONGOING_EVENT;
        nm.notify(NOTIFY_ID, notification);    	
    }

	private final ModInterface.Stub binder = new ModInterface.Stub() {
		public void play(String[] files, boolean shuffle, boolean loopList) {
			createNotification(null);
			fileArray = files;
			ridx = new RandomIndex(fileArray.length);
			shuffleMode = shuffle;
			loopListMode = loopList;
			returnToPrev = false;
			stopPlaying = false;
			paused = false;

			if (isPlaying) {
				Log.i("Xmp ModService", "Use existing player thread");
				restartList = true;
				nextSong();
			} else {
				Log.i("Xmp ModService", "Start player thread");
				restartList = false;
		   		playThread = new Thread(new PlayRunnable());
		   		playThread.start();
			}
			isPlaying = true;
		}
	    
	    public void stop() {
	    	xmp.stopModule();
	    	paused = false;
	    	stopPlaying = true;
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
			xmp.stopModule();
			stopPlaying = false;
			paused = false;
		}
		
		public void prevSong() {
			xmp.stopModule();
			returnToPrev = true;
			stopPlaying = false;
			paused = false;
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
		
		// for Reconnection
		
		public String getFileName() {
			return fileName;
		}
		
		public String[] getInstruments() {
			return xmp.getInstruments();
		}
		
		// Callback
		
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
