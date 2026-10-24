// SPDX-License-Identifier: GPL-3.0-or-later
package org.b0realis.reminiscence2;
import org.libsdl.app.SDLActivity;
import java.io.File;
public final class GameActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] {"SDL2", "main"}; }
    @Override protected String[] getArguments() {
        File root = getExternalFilesDir(null);
        File tunes = new File(root, "TUNES");
        File[] tracks = tunes.listFiles();
        if (tracks == null || tracks.length == 0) tunes = new File(root, "assets");
        return new String[] {"--datapath=" + new File(root, "assets"),
            "--savepath=" + new File(root, "SAVE"), "--tunepath=" + tunes, "--window"};
    }
}
