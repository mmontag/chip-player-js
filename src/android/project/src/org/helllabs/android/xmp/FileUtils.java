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
	public static void writeToFile(File file, String[] lines) throws IOException {
		BufferedWriter out = new BufferedWriter(new FileWriter(file, true));
		for (String s : lines) {
			out.write(s);
			out.newLine();
		}
		out.close();
	}
	
	public static void writeToFile(File file, String line) throws IOException {
		String[] lines = { line };
		writeToFile(file, lines);
	}
	
	public static String readFromFile(File file) throws IOException {
		String line = null;
		
		BufferedReader in = new BufferedReader(new FileReader(file), 512);
	    line = in.readLine();
	    in.close();
		
	    return line;
	}
	
	public static boolean removeLineFromFile(File inFile, int num)
	throws FileNotFoundException, IOException {
		int[] nums = { num };
		return removeLineFromFile(inFile, nums);
	}
	
	public static boolean removeLineFromFile(File inFile, int[] num)
	throws IOException, FileNotFoundException {

		File tempFile = new File(inFile.getAbsolutePath() + ".tmp");

		BufferedReader br = new BufferedReader(new FileReader(inFile), 512);
		PrintWriter pw = new PrintWriter(new FileWriter(tempFile));
		
		String line;
		boolean flag;
		for (int lineNum = 0; (line = br.readLine()) != null; lineNum++) {
			flag = false;
			for (int i = 0; i < num.length; i++) {
				if (lineNum == num[i]) {
					flag = true;
					break;
				}
			}
			if (!flag) {
				pw.println(line);
				pw.flush();
			}
		}
		pw.close();
		br.close();

		// Delete the original file
		if (!inFile.delete())
			return false;

		// Rename the new file to the filename the original file had.
		if (!tempFile.renameTo(inFile))
			return false;
			
		return true;
	}
}
