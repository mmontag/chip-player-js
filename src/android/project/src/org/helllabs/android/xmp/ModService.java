package org.helllabs.android.xmp;

import java.util.ArrayList;
import java.util.List;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;


public class ModService extends Service {
	private Xmp xmp = new Xmp();
	private int minSize = AudioTrack.getMinBufferSize(44100,
			AudioFormat.CHANNEL_CONFIGURATION_STEREO,
			AudioFormat.ENCODING_PCM_16BIT);
	private AudioTrack audio = new AudioTrack(
			AudioManager.STREAM_MUSIC, 44100,
			AudioFormat.CHANNEL_CONFIGURATION_STEREO,
			AudioFormat.ENCODING_PCM_16BIT,
			minSize < 4096 ? 4096 : minSize,
			AudioTrack.MODE_STREAM);
	private NotificationManager nm;
	private static final int NOTIFY_ID = R.layout.playlist;
	private List<String> songs = new ArrayList<String>();
	private int currentPosition;
    
	private class PlayRunnable implements Runnable {
    	public void run() {
       		while (xmp.playFrame() == 0) {
       			int size = xmp.softmixer();
       			short buffer[] = xmp.getBuffer(size);
       			int i = audio.write(buffer, 0, size / 2);
       			//Log.v(getString(R.string.app_name), "--> " + size + " " + i);
       		}
       		
       		audio.stop();
       		xmp.endPlayer();
       		xmp.releaseModule(); 
    	}
    }
	
    @Override
	public void onCreate() {
		//Log.v(getString(R.string.app_name), ">>> create service");    	
    	super.onCreate();
        nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        xmp.init();

    }

    @Override
	public void onDestroy() {
    	xmp.stopModule();
    	xmp.deinit();
    	nm.cancel(NOTIFY_ID);
    }

    @Override
    public IBinder onBind(Intent arg0) {
    	return mBinder;
    }
    
    private void playSong(String file) {
    	 /*Notification notification = new Notification(
                 R.drawable.playing, file, null, file, null);
    	 nm.notify(NOTIFY_ID, notification);*/
 
   		if (xmp.loadModule(songs.get(currentPosition)) < 0) {
   			return;
   		}
   		audio.play();
   		xmp.startPlayer();
   		
   		PlayRunnable playRunnable = new PlayRunnable();
   		Thread playThread = new Thread(playRunnable);
   		playThread.start();		
    }
    
    private void nextSong() {
        // Check if last song or not
        if (++currentPosition >= songs.size()) {
                currentPosition = 0;
                nm.cancel(NOTIFY_ID);
        } else {
                playSong(songs.get(currentPosition));
        }
}

    private void prevSong() {
        if (currentPosition >= 1) {
                playSong(songs.get(--currentPosition));
        } else {
                playSong(songs.get(currentPosition));
        }
    }
    
    private final Interface.Stub mBinder = new Interface.Stub() {

        public void playFile(int position) throws DeadObjectException {
        	try {
        		currentPosition = position;
        		Log.v(getString(R.string.app_name), "play song: " + songs.get(position));
        		playSong(songs.get(position));

        	} catch (IndexOutOfBoundsException e) {
        		Log.e(getString(R.string.app_name), e.getMessage());
        	}
        }

        public void addSongPlaylist(String song) throws DeadObjectException {
        	//Log.v(getString(R.string.app_name), "add song: " + song);
        	songs.add(song);
        }

        public void clearPlaylist() throws DeadObjectException {
        	songs.clear();
        }

        public void skipBack() throws DeadObjectException {
        	prevSong();

        }

        public void skipForward() throws DeadObjectException {
        	nextSong();
        }

        public void pause() throws DeadObjectException {
        	/*Notification notification = new Notification(
        			R.drawable.paused, null, null, null, null);
        	nm.notify(NOTIFY_ID, notification);*/
        	//mp.pause();
        }

        public void stop() throws DeadObjectException {
        	//nm.cancel(NOTIFY_ID);
        	xmp.stopModule();
        }
        
        public int time() throws DeadObjectException {
        	return xmp.time();
        }

		public void seekPosition(int seconds) throws DeadObjectException {
			xmp.seek(seconds);
		}
    };
}

