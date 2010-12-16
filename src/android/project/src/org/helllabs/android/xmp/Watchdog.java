package org.helllabs.android.xmp;

public class Watchdog implements Runnable {
	private int timer;
	private boolean running;
	private Thread thread;
	private onTimeoutListener listener;
	private int timeout;
	
	public Watchdog(int timeout) {
		this.timeout = timeout; 
	}
	
	public interface onTimeoutListener {
		abstract void onTimeout();
	}
	
	public void setOnTimeoutListener(onTimeoutListener l) {
		listener = l;
	}
	
	public void run() {
		while (running) {
			if (--timer <= 0) {
				listener.onTimeout();
				break;
			}
			
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {	}
		}
	}
	
	public void start() {
		running = true;
		refresh();
		thread = new Thread(this);
		thread.start();
	}
	
	public void stop() {
		running = false;
		try {
			thread.join();
		} catch (InterruptedException e) { }
	}
	
	public void refresh() {
		timer = timeout;
	}
}
