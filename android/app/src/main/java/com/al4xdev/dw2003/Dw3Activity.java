package com.al4xdev.dw2003;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.View;
import android.widget.ProgressBar;

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
        return new String[] { "SDL3", "main" };
    }

    private File resolveAppDataDir() {
        File ext = getExternalFilesDir(null);
        if (ext != null) return ext;
        return getFilesDir();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        File dir = resolveAppDataDir();
        if (dir != null) {
            dir.mkdirs();
            new File(dir, "game").mkdirs();
            new File(dir, "logs").mkdirs();
            try {
                nativeSetFilesDir(dir.getAbsolutePath());
            } catch (Throwable ignored) {}
        }
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

    private long querySize(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[]{OpenableColumns.SIZE}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.SIZE);
                if (idx >= 0 && !c.isNull(idx)) return c.getLong(idx);
            }
        } catch (Throwable ignored) {}
        return -1;
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

        // The copy of a ~660 MB image takes from seconds to a minute; the launcher cannot draw while it waits, so the
        // progress is a dialog of the activity's own.
        long total = 0;
        for (Uri u : uris) {
            long n = querySize(u);
            if (n > 0) total += n;
        }
        final long totalBytes = total;
        final ProgressBar bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setIndeterminate(totalBytes <= 0);
        bar.setMax(1000);
        bar.setPadding(48, 32, 48, 32);
        final AlertDialog dialog = new AlertDialog.Builder(this)
            .setTitle("Copying the disc image")
            .setView(bar)
            .setCancelable(false)
            .create();
        dialog.show();

        new Thread(() -> {
            boolean anyCopied = false;
            File gameDir = new File(resolveAppDataDir(), "game");
            gameDir.mkdirs();
            byte[] buf = new byte[1 << 20];
            long done = 0;
            int shown = -1;
            for (Uri u : uris) {
                String name = queryDisplayName(u).replaceAll("[/\\\\]", "_");
                File out = new File(gameDir, name);
                // Into a .part first: a copy cut short (no space, the file vanished) must not leave a truncated image
                // where the launcher looks for the disc.
                File part = new File(gameDir, name + ".part");
                boolean ok = false;
                try (InputStream in = getContentResolver().openInputStream(u);
                     FileOutputStream fos = new FileOutputStream(part)) {
                    if (in == null) continue;
                    int r;
                    while ((r = in.read(buf)) > 0) {
                        fos.write(buf, 0, r);
                        done += r;
                        int p = totalBytes > 0 ? (int)(done * 1000 / totalBytes) : 0;
                        if (p != shown) {
                            shown = p;
                            final int pp = p;
                            runOnUiThread(() -> bar.setProgress(pp));
                        }
                    }
                    ok = true;
                } catch (Throwable ignored) {}
                // A re-import replaces the image: rename does not overwrite on every storage, so the old one goes
                // first.
                if (ok && (!out.exists() || out.delete()) && part.renameTo(out)) {
                    anyCopied = true;
                } else {
                    part.delete();
                }
            }
            final boolean copied = anyCopied;
            runOnUiThread(() -> {
                try { dialog.dismiss(); } catch (Throwable ignored) {}
                // Without a word the launcher's own file browser opens next, which reads as the picker not working.
                if (!copied) {
                    android.widget.Toast.makeText(this, "Could not copy the disc image into the app",
                        android.widget.Toast.LENGTH_LONG).show();
                }
            });
            nativeOnDiscPicked(anyCopied);
        }).start();
    }
}
