package org.helllabs.android.xmp;

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.ArrayAdapter;

public class ListFormats extends ListActivity {
	private Xmp xmp = new Xmp();
		
	String[] formats = xmp.getFormats();
	
	@Override
    public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.list_formats);
		setListAdapter(new ArrayAdapter(this, 
				R.layout.format_list_item, formats));
	}
}
