package org.helllabs.android.xmp;

import java.util.List;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

public class PlaylistInfoAdapter extends ArrayAdapter<PlaylistInfo> {
    private List<PlaylistInfo> items;
    private Context myContext;

    public PlaylistInfoAdapter(Context context, int resource, int textViewResourceId, List<PlaylistInfo> items) {
    	super(context, resource, textViewResourceId, items);
    	this.items = items;
    	this.myContext = context;
    }
    
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
    	View v = convertView;
    	if (v == null) {
    		LayoutInflater vi = (LayoutInflater)myContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    		v = vi.inflate(R.layout.playlist_item, null);
    	}
    	PlaylistInfo o = items.get(position);
            
    	if (o != null) {                		
    		TextView tt = (TextView)v.findViewById(R.id.plist_title);
    		TextView bt = (TextView)v.findViewById(R.id.plist_info);
    		ImageView im = (ImageView)v.findViewById(R.id.plist_image);
    		if (tt != null) {
    			tt.setText(o.name);
    		}
    		if (bt != null) {
    			bt.setText(o.comment);
    		}
    		if (im != null && o.imageRes > 0) {
    			im.setImageResource(o.imageRes);
    		}
    	}
            
    	return v;
    }
}
