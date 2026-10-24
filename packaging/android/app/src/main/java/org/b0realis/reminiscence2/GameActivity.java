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
        // Let the engine isolate native music to the chosen edition. Passing the
        // whole assets tree as an explicit tune path merges unrelated editions.
        if (tracks == null || tracks.length == 0)
            return new String[] {"--datapath=" + new File(root, "assets"),
                "--savepath=" + new File(root, "SAVE"), "--window"};
        return new String[] {"--datapath=" + new File(root, "assets"),
            "--savepath=" + new File(root, "SAVE"), "--tunepath=" + tunes, "--window"};
    }
}
