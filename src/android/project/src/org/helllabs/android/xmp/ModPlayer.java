package org.helllabs.android.xmp;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.preference.PreferenceManager;


public class ModPlayer {
	Xmp xmp = new Xmp();
	AudioTrack audio;
	boolean paused;
	Thread playThread;
	SharedPreferences prefs;
	int minSize;
	boolean stereo;
	boolean interpolate;
	
	public ModPlayer(Context context) {	
   		prefs = PreferenceManager.getDefaultSharedPreferences(context);
   		
   		int sampleRate = Integer.parseInt(prefs.getString(Settings.PREF_SAMPLING_RATE, "44100"));
   		int bufferMs = prefs.getInt(Settings.PREF_BUFFER_MS, 150);
   		stereo = prefs.getBoolean(Settings.PREF_STEREO, true);
   		interpolate = prefs.getBoolean(Settings.PREF_INTERPOLATION, true);
   		
   		int bufferSize = sampleRate * (stereo ? 2 : 1) * 2 * bufferMs / 1000;
   			
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
		
		xmp.init(sampleRate);
		paused = false;
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
    	}
    }


	public void end() {	
    	xmp.stopModule();
    	paused = false;
    	try {
			playThread.join();
		} catch (InterruptedException e) { }
		xmp.deinit();
    	audio.release();
    }
   
    public void play(String file) {
		xmp.optInterpolation(prefs.getBoolean(Settings.PREF_INTERPOLATION, true));
		xmp.optFilter(prefs.getBoolean(Settings.PREF_FILTER, true));
		
   		if (xmp.loadModule(file) < 0) {
   			return;
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

