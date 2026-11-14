package com.al4xdev.dw2003;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class Dw3Activity extends SDLActivity {
    private static final int REQ_PICK_DISC = 9001;

    private static native void nativeSetFilesDir(String dir);
    private static native void nativeOnDiscPicked(boolean ok);

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    private File resolveAppDataDir() {
        File ext = getExternalFilesDir(null);
        if (ext != null) return ext;
        return getFilesDir();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File dir = resolveAppDataDir();
        if (dir != null) {
            dir.mkdirs();
            new File(dir, "game").mkdirs();
            new File(dir, "logs").mkdirs();
            nativeSetFilesDir(dir.getAbsolutePath());
        }
        super.onCreate(savedInstanceState);
        enableImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enableImmersiveMode();
    }

    private void enableImmersiveMode() {
        try {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        } catch (Throwable ignored) {}
    }

    public void pickDiscFiles() {
        runOnUiThread(() -> {
            try {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                startActivityForResult(Intent.createChooser(i, "Select Digimon World 3 (.cue / .bin)"), REQ_PICK_DISC);
            } catch (Throwable t) {
                nativeOnDiscPicked(false);
            }
        });
    }

    private String queryDisplayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) return c.getString(idx);
            }
        } catch (Throwable ignored) {}
        String last = uri.getLastPathSegment();
        if (last == null) return "disc.bin";
        int slash = last.lastIndexOf('/');
        return slash >= 0 ? last.substring(slash + 1) : last;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_DISC) return;

        if (resultCode != Activity.RESULT_OK || data == null) {
            nativeOnDiscPicked(false);
            return;
        }

        final List<Uri> uris = new ArrayList<>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u != null) uris.add(u);
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }

        if (uris.isEmpty()) {
            nativeOnDiscPicked(false);
            return;
        }

        new Thread(() -> {
            boolean anyCopied = false;
            File gameDir = new File(resolveAppDataDir(), "game");
            gameDir.mkdirs();
            byte[] buf = new byte[65536];
            for (Uri u : uris) {
                String name = queryDisplayName(u).replaceAll("[/\\\\]", "_");
                File out = new File(gameDir, name);
                try (InputStream in = getContentResolver().openInputStream(u);
                     FileOutputStream fos = new FileOutputStream(out)) {
                    if (in == null) continue;
                    int r;
                    while ((r = in.read(buf)) > 0) fos.write(buf, 0, r);
                    anyCopied = true;
                } catch (Throwable ignored) {}
            }
            nativeOnDiscPicked(anyCopied);
        }).start();
    }
}
