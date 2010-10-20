package org.helllabs.android.xmp;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.preference.PreferenceManager;


public class ModPlayer {
	private static ModPlayer instance = null;
	private Xmp xmp = new Xmp();
	private int minSize;
	private int sampleRate;
	private int bufferSize;
	private AudioTrack audio;
	private boolean paused;
	private Thread playThread;
	private SharedPreferences prefs;
	
	protected ModPlayer(Context context) {	
   		prefs = PreferenceManager.getDefaultSharedPreferences(context);
   		
   		sampleRate = 44100;
   		bufferSize = 4096;
   		
		minSize = AudioTrack.getMinBufferSize(
				sampleRate,
				AudioFormat.CHANNEL_CONFIGURATION_STEREO,
				AudioFormat.ENCODING_PCM_16BIT);
		
		audio = new AudioTrack(
				AudioManager.STREAM_MUSIC, sampleRate,
				AudioFormat.CHANNEL_CONFIGURATION_STEREO,
				AudioFormat.ENCODING_PCM_16BIT,
				minSize < bufferSize ? bufferSize : minSize,
				AudioTrack.MODE_STREAM);
		
		xmp.init(sampleRate);
		paused = false;
	}
	
	public static ModPlayer getInstance(Context context) {
		if(instance == null) {
			instance = new ModPlayer(context);
		}
		return instance;
	}
	    
	private class PlayRunnable implements Runnable {
		int count = 0;
		
    	public void run() {
    		short buffer[] = new short[minSize];
       		while (xmp.playFrame() == 0) {
       			if (++count > 10) {
       				String volBoost = prefs.getString(Settings.PREF_VOL_BOOST, "1");
       				xmp.optAmplify(Integer.parseInt(volBoost));
       				xmp.optMix(prefs.getBoolean(Settings.PREF_STEREO, true) ?
       						prefs.getInt(Settings.PREF_PAN_SEPARATION, 70): 0);
       				count = 0;
       			}
       			       			
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
    	}
    }


	protected void finalize() {
    	xmp.stopModule();
    	paused = false;
    	try {
			playThread.join();
		} catch (InterruptedException e) { }

    	xmp.deinit();
    }
   
    public void play(String file) {
   		if (xmp.loadModule(file) < 0) {
   			return;
   		}

   		audio.play();
   		xmp.startPlayer();
   		
   		PlayRunnable playRunnable = new PlayRunnable();
   		playThread = new Thread(playRunnable);
   		playThread.start();
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
	
	public String[] getInstruments() {
		return xmp.getInstruments();
	}
	
	public void getVolumes(int[] volumes) {
		xmp.getVolumes(volumes);
	}
}

