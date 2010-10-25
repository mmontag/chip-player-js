package org.helllabs.android.xmp;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

public class FileUtils {
	public static int writeToFile(File file, String[] lines) {
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(file, true));
			for (String s : lines) {
				out.write(s);
				out.newLine();
			}
			out.close();
			return 0;
		} catch (IOException e) {
			return -1;
		}
	}
	
	public static int writeToFile(File file, String line) {
		String[] lines = { line };
		return writeToFile(file, lines);
	}
	
	public static String readFromFile(File file) {
		String line = null;
		
		try {
			BufferedReader in = new BufferedReader(new FileReader(file));
	    	line = in.readLine();
	    	in.close();
	    } catch (IOException e) {
	    	 
	    }
		
	    return line;
	}

	public static int removeLineFromFile(File inFile, int num) {

		try {
			if (!inFile.isFile())
				return -1;

			File tempFile = new File(inFile.getAbsolutePath() + ".tmp");

			BufferedReader br = new BufferedReader(new FileReader(inFile));
			PrintWriter pw = new PrintWriter(new FileWriter(tempFile));

			String line;
			for (int lineNum = 0; (line = br.readLine()) != null; lineNum++) {
				if (lineNum != num) {
					pw.println(line);
					pw.flush();
				}
			}
			pw.close();
			br.close();

			// Delete the original file
			if (!inFile.delete())
				return -1;

			// Rename the new file to the filename the original file had.
			if (!tempFile.renameTo(inFile))
				return -1;
			
			return 0;

		} catch (FileNotFoundException e) {
			return -1;
		} catch (IOException e) {
			return -1;
		}
	}
}
