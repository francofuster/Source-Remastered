package io.github.francofuster.zeq2remaster;

import android.content.Context;
import android.content.Intent;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Entry point of the APK. SDLActivity starts the engine, libmain.so, on its
 * own thread; the three game modules are loaded later by Sys_LoadDll.
 */
public class ZEQ2Activity extends SDLActivity {

    /** Render height when the config does not say (r_androidHeight). */
    private static final int DEFAULT_RENDER_HEIGHT = 720;
    private static final Pattern RENDER_HEIGHT =
        Pattern.compile("^\\s*seta?\\s+r_androidHeight\\s+\"?(\\d+)\"?", Pattern.MULTILINE);

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return new ZEQ2Surface(context, renderHeight());
    }

    /**
     * r_androidHeight from the saved config: the surface is created before
     * the engine starts, so a change applies on the next launch. 0 renders
     * at the screen resolution.
     */
    private int renderHeight() {
        Intent intent = getIntent();
        // "+set r_androidHeight N" in the adb arguments wins, for testing
        String extra = intent != null ? intent.getStringExtra("args") : null;
        if (extra != null) {
            Matcher m = Pattern.compile("r_androidHeight\\s+(\\d+)").matcher(extra);
            if (m.find()) {
                return Integer.parseInt(m.group(1));
            }
        }
        String root = intent != null ? intent.getStringExtra(SetupActivity.EXTRA_DATA_ROOT) : null;
        if (root != null) {
            File config = new File(root, "ZEQ2/zeq2config.cfg");
            try (BufferedReader in = new BufferedReader(new FileReader(config))) {
                StringBuilder text = new StringBuilder();
                String line;
                while ((line = in.readLine()) != null) {
                    text.append(line).append('\n');
                }
                Matcher m = RENDER_HEIGHT.matcher(text);
                if (m.find()) {
                    return Integer.parseInt(m.group(1));
                }
            } catch (IOException | NumberFormatException ignored) {
                // no config yet (first launch) or a bad value: the default
            }
        }
        return DEFAULT_RENDER_HEIGHT;
    }

    /**
     * Engine command line. Extra arguments can be passed when starting the
     * activity from adb, for testing:
     *   adb shell am start -n io.github.francofuster.zeq2remaster/.ZEQ2Activity --es args "+map lastStand"
     */
    @Override
    protected String[] getArguments() {
        // logfile 2: keep qconsole.log flushed so a crash does not lose the end.
        List<String> args = new ArrayList<>(Arrays.asList("+set", "logfile", "2"));

        Intent intent = getIntent();

        // The folder SetupActivity unpacked the data into
        String root = intent != null ? intent.getStringExtra(SetupActivity.EXTRA_DATA_ROOT) : null;
        if (root != null) {
            args.addAll(Arrays.asList("+set", "fs_basepath", root, "+set", "fs_homepath", root));
        }

        String extra = intent != null ? intent.getStringExtra("args") : null;
        if (extra != null && !extra.trim().isEmpty()) {
            args.addAll(Arrays.asList(extra.trim().split("\\s+")));
        }
        return args.toArray(new String[0]);
    }
}
