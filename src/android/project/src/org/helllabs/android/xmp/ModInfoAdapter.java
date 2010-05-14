package org.helllabs.android.xmp;

import java.util.List;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

public class ModInfoAdapter extends ArrayAdapter<ModInfo> {
    private List<ModInfo> items;
    private Context myContext;

    public ModInfoAdapter(Context context, int resource, int textViewResourceId, List<ModInfo> items) {
    	super(context, resource, textViewResourceId, items);
    	this.items = items;
    	this.myContext = context;
    }
    
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
    	View v = convertView;
    	if (v == null) {
    		LayoutInflater vi = (LayoutInflater)myContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    		v = vi.inflate(R.layout.song_item, null);
    	}
    	ModInfo o = items.get(position);
            
    	if (o != null) {                		
    		TextView tt = (TextView) v.findViewById(R.id.title);
    		TextView bt = (TextView) v.findViewById(R.id.info);
    		if (tt != null) {
    			tt.setText(o.name);
    		}
    		if(bt != null){
    			bt.setText(o.chn + " chn " + o.type);
    		}
    	}
            
    	return v;
    }
}
