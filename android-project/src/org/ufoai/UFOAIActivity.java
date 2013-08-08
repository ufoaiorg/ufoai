package org.ufoai;

import org.libsdl.app.SDLActivity;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.widget.RelativeLayout;

public class UFOAIActivity extends SDLActivity {
	protected static final String NAME = UFOAIActivity.class.getSimpleName();
	private static RelativeLayout layout;
	protected static Handler uiHandler;

	private static final int SDLViewID = 1;

	@Override
	public void setContentView(final View view) {
		layout = new RelativeLayout(this);
		view.setId(SDLViewID);
		layout.addView(view);
		super.setContentView(layout);
	}

	@Override
	protected void onCreate(final Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		uiHandler = new Handler();
		final DisplayMetrics dm = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(dm);
	}

	public static void openURL(final String url) {
		final Intent browserIntent = new Intent(Intent.ACTION_VIEW,
				Uri.parse(url));
		mSingleton.startActivity(browserIntent);
	}

	static void alert(final String message) {
		final AlertDialog.Builder bld = new AlertDialog.Builder(mSingleton);
		bld.setMessage(message);
		bld.setNeutralButton("OK", null);
		Log.d(NAME, "Showing alert dialog: " + message);
		bld.create().show();
	}

	static boolean isOUYA() {
		try {
			mSingleton.getPackageManager().getPackageInfo("tv.ouya", 0);
			return true;
		} catch (final NameNotFoundException ignore) {
		}
		return false;
	}
}
