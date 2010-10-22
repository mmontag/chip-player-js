package org.helllabs.android.xmp;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.widget.EditText;

public class PlayList extends ListActivity {
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.playlist);
		
	}
	
	public static void addNew(Context context) {
		AlertDialog.Builder alert = new AlertDialog.Builder(context);  
		  
		alert.setTitle("New playlist");  
		alert.setMessage("Enter the new playlist name:");  
		     
		final EditText input = new EditText(context);  
		alert.setView(input);  
		  
		alert.setPositiveButton("Ok", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {  
				//String value = input.getText();  
			}  
		});  
		  
		alert.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {  
			public void onClick(DialogInterface dialog, int whichButton) {  
				// Canceled.  
			}  
		});  
		  
		alert.show(); 
	}
}
