package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;


public class InfoCache {
	static Xmp xmp = new Xmp();
	
	public static boolean testModule(String filename) {
		File file = new File(filename);
		File cacheFile = new File(Settings.cacheDir, filename + ".cache");

		if (!Settings.cacheDir.isDirectory()) {
			if (Settings.cacheDir.mkdirs() == false) {
				// Can't use cache
				return xmp.testModule(filename);
			}
		}
		
		try {
			// If cache file exists and size matches, file is mod
			if (cacheFile.isFile()) {
				int size = Integer.parseInt(FileUtils.readFromFile(cacheFile));
				if (size == file.length())
					return true;
			}
			
			return xmp.testModule(filename);
		} catch (IOException e) {
			return xmp.testModule(filename);
		}	
	}
	
	public static ModInfo getModInfo(String filename) {
		File file = new File(filename);
		File cacheFile = new File(Settings.cacheDir, filename + ".cache");
		ModInfo mi;

		if (!Settings.cacheDir.isDirectory()) {
			if (Settings.cacheDir.mkdirs() == false) {
				// Can't use cache
				return xmp.getModInfo(filename);
			}
		}

		try {
			// If cache file exists and size matches, file is mod
			if (cacheFile.isFile()) {
				BufferedReader in = new BufferedReader(new FileReader(cacheFile));			
				int size = Integer.parseInt(in.readLine());
				if (size == file.length()) {
					mi = new ModInfo();
					
					mi.name = in.readLine();
					mi.filename = in.readLine();
					mi.type = in.readLine();
					mi.chn = Integer.parseInt(in.readLine());
					mi.pat = Integer.parseInt(in.readLine());
					mi.ins = Integer.parseInt(in.readLine());
					mi.trk = Integer.parseInt(in.readLine());
					mi.smp = Integer.parseInt(in.readLine());
					mi.len = Integer.parseInt(in.readLine());
					mi.bpm = Integer.parseInt(in.readLine());
					mi.tpo = Integer.parseInt(in.readLine());
					mi.time = Integer.parseInt(in.readLine());
					
					in.close();
					return mi;
				}
				in.close();
			}

			if ((mi = xmp.getModInfo(filename)) != null) {
				String[] lines = {
					Long.toString(file.length()),
					mi.name,
					mi.filename,
					mi.type,
					Integer.toString(mi.chn),
					Integer.toString(mi.pat),
					Integer.toString(mi.ins),
					Integer.toString(mi.trk),
					Integer.toString(mi.smp),
					Integer.toString(mi.len),
					Integer.toString(mi.bpm),
					Integer.toString(mi.tpo),
					Integer.toString(mi.time)
				};
				
				File dir = cacheFile.getParentFile();
				if (!dir.isDirectory())
					dir.mkdirs();			
				cacheFile.createNewFile();
				FileUtils.writeToFile(cacheFile, lines);
				
				return mi;
			}
			
			return null;
		} catch (IOException e) {
			return xmp.getModInfo(filename);
		}
	}
}
