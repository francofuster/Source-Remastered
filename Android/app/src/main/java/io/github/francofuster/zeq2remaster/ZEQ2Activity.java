package io.github.francofuster.zeq2remaster;

import android.content.Intent;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Entry point of the APK. SDLActivity starts the engine, libmain.so, on its
 * own thread; the three game modules are loaded later by Sys_LoadDll.
 */
public class ZEQ2Activity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
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
        String extra = intent != null ? intent.getStringExtra("args") : null;
        if (extra != null && !extra.trim().isEmpty()) {
            args.addAll(Arrays.asList(extra.trim().split("\\s+")));
        }
        return args.toArray(new String[0]);
    }
}
