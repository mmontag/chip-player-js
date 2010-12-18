package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

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
	Notifier notifier;
	boolean shuffleMode = true;
	boolean loopListMode = false;
	boolean stopPlaying = false;
	static boolean isPlaying = false;
	boolean restartList;
	boolean returnToPrev;
	boolean paused;
	String fileName;			// currently playing file
	String currentTitle;
    ArrayList<String> fileArray = null;
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
		
		notifier = new Notifier();
		
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
    	notifier.cancel();
    	end();
    }

	@Override
	public IBinder onBind(Intent intent) {
		return binder;
	}
	
	private class PlayRunnable implements Runnable {
    	public void run() {
    		do {
	    		for (int index = 0; index < fileArray.size(); index++) {
		    		Log.i("Xmp ModService", "Load " + fileArray.get(index));
		       		if (xmp.loadModule(fileArray.get(index)) < 0)
		       			continue;

		       		fileName = fileArray.get(index);
		       		notifier.notification(xmp.getTitle(), index);
		       		
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
		       					index += fileArray.size();
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
	    			Collections.shuffle(fileArray);
    		} while (loopListMode);

    		watchdog.stop();
    		notifier.cancel();
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
	
	private class Notifier {
	    private NotificationManager nm;
	    PendingIntent contentIntent;
	    private static final int NOTIFY_ID = R.layout.player;
		String title;
		int index;

		public Notifier() {
			nm = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
	    	contentIntent = PendingIntent.getActivity(ModService.this, 0,
					new Intent(ModService.this, Player.class), 0);
		}
		
		private String message() {
			return fileArray.size() > 1 ?
				String.format("%s (%d/%d)", title, index, fileArray.size()) :
				title;
		}
		
		public void cancel() {
			nm.cancel(NOTIFY_ID);
		}
		
		public void notification() {
			notification(null, null);
		}
		
		public void notification(String title, int index) {
			this.title = title;
			this.index = index + 1;			
			notification(message(), message());
		}
		
		public void notification(String ticker) {
			notification(ticker, message());
		}
		
		public void notification(String ticker, String latest) {
	        Notification notification = new Notification(
	        		R.drawable.notification, ticker, System.currentTimeMillis());
	        notification.setLatestEventInfo(ModService.this, getText(R.string.app_name),
	        		latest, contentIntent);
	        notification.flags |= Notification.FLAG_ONGOING_EVENT;	        
	        nm.notify(NOTIFY_ID, notification);				
		}
	}

	private final ModInterface.Stub binder = new ModInterface.Stub() {
		public void play(String[] files, boolean shuffle, boolean loopList) {	
			notifier.notification();
			fileArray = new ArrayList<String>(Arrays.asList(files));
			shuffleMode = shuffle;
			loopListMode = loopList;
			returnToPrev = false;
			stopPlaying = false;
			paused = false;
			
			if (shuffleMode)
				Collections.shuffle(fileArray);

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
		
		public void add(String[] files) {				
			for (String s : files)
				fileArray.add(s);
			notifier.notification("Added to play queue");			
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
