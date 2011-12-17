package org.helllabs.android.xmp;

import org.helllabs.android.xmp.Watchdog.onTimeoutListener;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.IBinder;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.KeyEvent;


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
	boolean stopPlaying = false;
	static boolean isPlaying = false;
	boolean restartList;
	boolean returnToPrev;
	boolean paused;
	String fileName;			// currently playing file
	String currentTitle;
	QueueManager queue;
    final RemoteCallbackList<PlayerCallback> callbacks =
		new RemoteCallbackList<PlayerCallback>();
    boolean autoPaused = false;		// paused on phone call
    XmpPhoneStateListener listener;
    TelephonyManager tm;
    
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
		listener = new XmpPhoneStateListener(this);
		
		tm = (TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE);
		tm.listen(listener, XmpPhoneStateListener.LISTEN_CALL_STATE);
		
		watchdog = new Watchdog(10);
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
	
	private void checkMediaButtons() {
		int key = RemoteControlReceiver.keyCode;
		
		if (key > 0) {
			switch (key) {
			case KeyEvent.KEYCODE_MEDIA_NEXT:
				Log.i("Xmp ModService", "Handle KEYCODE_MEDIA_NEXT");
				xmp.stopModule();
				stopPlaying = false;
				paused = false;
				break;
			case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
				Log.i("Xmp ModService", "Handle KEYCODE_MEDIA_PREVIOUS");
				if (xmp.time() > 20) {
					xmp.seek(0);
				} else {
					xmp.stopModule();
					returnToPrev = true;
					stopPlaying = false;
				}
				paused = false;
				break;
			case KeyEvent.KEYCODE_MEDIA_STOP:
				Log.i("Xmp ModService", "Handle KEYCODE_MEDIA_STOP");
		    	xmp.stopModule();
		    	paused = false;
		    	stopPlaying = true;
		    	break;
			case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
				Log.i("Xmp ModService", "Handle KEYCODE_MEDIA_PLAY_PAUSE");
				paused = !paused;
				break;
			}
			
			RemoteControlReceiver.keyCode = -1;
		}
	}
	
	private class PlayRunnable implements Runnable {
    	public void run() {
    		do {
	    		Log.i("Xmp ModService", "Load " + queue.getFilename());
	       		if (xmp.loadModule(queue.getFilename()) < 0)
	       			continue;

	       		fileName = queue.getFilename();
	       		notifier.notification(xmp.getTitle(), queue.index());
	       		
	    		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
	    		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));
		       		    	
	        	final int numClients = callbacks.beginBroadcast();
	        	for (int j = 0; j < numClients; j++) {
	        		try {
	    				callbacks.getBroadcastItem(j).newModCallback(
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
							checkMediaButtons();
						} catch (InterruptedException e) {
							break;
						}
	       			}
	       			
	       			watchdog.refresh();
	       			checkMediaButtons();
	       		}

	       		audio.stop();
	       		xmp.endPlayer();
	       		xmp.releaseModule();
	       		
	       		if (restartList) {
	       			queue.restart();
	       			restartList = false;
	       			continue;
	       		}
	       		
	       		if (returnToPrev) {
	       			queue.previous();
	       			returnToPrev = false;
	       			continue;
	       		}
    		} while (!stopPlaying && queue.next());

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
    	if (playThread != null && playThread.isAlive()) {
    		try {
    			playThread.join();
    		} catch (InterruptedException e) { }
    	}
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
			return queue.size() > 1 ?
				String.format("%s (%d/%d)", title, index, queue.size()) :
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
			queue = new QueueManager(files, shuffle, loopList);
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
		
		public void add(String[] files) {	
			queue.add(files);
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
		
		public void getChannelData(int[] volumes, int[] instruments, int[] keys) {
			xmp.getChannelData(volumes, instruments, keys);
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
		
		public boolean isPaused() {
			return paused;
		}
		
		// for Reconnection
		
		public String getFileName() {
			return fileName;
		}
		
		public String[] getInstruments() {
			return xmp.getInstruments();
		}
		
		
		// File management
		
		public boolean deleteFile() {
			Log.i("Xmp ModService", "Delete file " + fileName);
			return InfoCache.delete(fileName);
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
	
	
	// for Telephony
	
	public boolean autoPause(boolean pause) {
		Log.i("Xmp ModService", "Auto pause changed to " + pause + ", previously " + autoPaused);
		if (pause) {
			paused = autoPaused = true;
		} else {
			if (autoPaused) {
				paused = autoPaused = false;
			}
		}	
		
		return autoPaused;
	}
}
