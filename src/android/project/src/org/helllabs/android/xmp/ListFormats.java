package org.helllabs.android.xmp;

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.ArrayAdapter;

public class ListFormats extends ListActivity {
	String[] formats = { "a", "b", "c" };
	
	@Override
    public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.list_formats);
		setListAdapter(new ArrayAdapter(this, 
				android.R.layout.simple_list_item_1, formats));
	}
}
