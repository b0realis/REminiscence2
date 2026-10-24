// SPDX-License-Identifier: GPL-3.0-or-later
package org.b0realis.reminiscence2;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;

/** Standard 2D Android launcher: no VR scene, broad storage permission or game data. */
public final class LauncherActivity extends Activity {
    private File root;
    private TextView status;
    private static final Set<String> MARKERS = new HashSet<>(Arrays.asList(
        "level1.lev", "demo.lev", "level1.map", "level1.bnq", "glob_fr.aba", "demo_uk.aba",
        "intro.seq", "menu1ssi.map", "flashback.bin", "flashback.rsrc", "global.paq", "present.lev"));
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        root = getExternalFilesDir(null);
        if (root == null) { finish(); return; }
        for (String name : new String[]{"assets", "SAVE", "TUNES"}) new File(root, name).mkdirs();
        if (hasData(new File(root, "assets"), 0)) { play(); return; }
        LinearLayout panel = new LinearLayout(this); panel.setOrientation(LinearLayout.VERTICAL); panel.setPadding(32,32,32,32);
        status = new TextView(this); status.setTextSize(20);
        status.setText("REminiscence2\n\nImport a ZIP containing your prepared Flashback game files, or use the Linux ADB deployment tool. Amiga WHDLoad archives must be extracted on your computer first.\n\nUse a Bluetooth gamepad or keyboard to play. F10, Escape or gamepad Start opens settings.");
        panel.addView(status);
        Button choose = new Button(this); choose.setText("Import prepared assets ZIP");
        choose.setOnClickListener(v -> { Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT); pick.setType("application/zip"); pick.addCategory(Intent.CATEGORY_OPENABLE); startActivityForResult(pick, 1); });
        panel.addView(choose);
        Button launch = new Button(this); launch.setText("Play / check assets again");
        launch.setOnClickListener(v -> { if (hasData(new File(root,"assets"),0)) play(); else status.append("\nNo supported game files found yet."); });
        panel.addView(launch); setContentView(panel);
    }
    private void play() { startActivity(new Intent(this, GameActivity.class)); finish(); }
    private boolean hasData(File directory, int depth) {
        if (depth > 64) return false;
        File[] files = directory.listFiles(); if (files == null) return false;
        for (File f : files) {
            if (f.isDirectory() ? hasData(f, depth+1) : MARKERS.contains(f.getName().toLowerCase(Locale.ROOT))) return true;
        }
        return false;
    }
    private void removeTree(File file) { File[] children = file.listFiles(); if (children != null) for (File c : children) removeTree(c); file.delete(); }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != 1 || result != RESULT_OK || data == null || data.getData() == null) return;
        status.setText("Importing game files...");
        final android.net.Uri uri = data.getData();
        new Thread(() -> {
            File staging = new File(root, "assets.import-" + UUID.randomUUID());
            File previous = new File(root, "assets.previous-" + UUID.randomUUID());
            try {
                if (!staging.mkdir()) throw new IOException("Cannot create import folder");
                long total = 0; int count = 0;
                try (InputStream source = getContentResolver().openInputStream(uri); ZipInputStream zip = new ZipInputStream(source)) {
                    ZipEntry entry; byte[] buffer = new byte[65536];
                    while ((entry = zip.getNextEntry()) != null) {
                        String name = entry.getName();
                        if (++count > 10000 || name.startsWith("/") || name.contains("\\") || name.contains(":")) throw new IOException("Unsafe ZIP entry");
                        File output = new File(staging, name);
                        if (!output.getCanonicalPath().startsWith(staging.getCanonicalPath() + File.separator)) throw new IOException("Unsafe ZIP path");
                        if (entry.isDirectory()) { output.mkdirs(); continue; }
                        output.getParentFile().mkdirs();
                        if (output.exists()) throw new IOException("Duplicate ZIP entry");
                        try (FileOutputStream stream = new FileOutputStream(output)) {
                            int read; while ((read = zip.read(buffer)) != -1) {
                                total += read; if (total > 1024L * 1024 * 1024) throw new IOException("Import exceeds 1 GiB");
                                stream.write(buffer,0,read);
                            }
                            stream.getFD().sync();
                        }
                    }
                }
                if (!hasData(staging,0)) throw new IOException("ZIP has no supported prepared game files");
                File assets = new File(root,"assets");
                if (!assets.renameTo(previous)) throw new IOException("Cannot preserve previous assets");
                if (!staging.renameTo(assets)) { previous.renameTo(assets); throw new IOException("Cannot install imported assets"); }
                removeTree(previous);
                runOnUiThread(this::play);
            } catch (Exception error) { runOnUiThread(() -> status.setText("Import failed: " + error.getMessage())); }
            finally { removeTree(staging); }
        }, "asset-import").start();
    }
}
