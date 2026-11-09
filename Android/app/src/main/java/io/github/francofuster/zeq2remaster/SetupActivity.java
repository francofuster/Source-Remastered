package io.github.francofuster.zeq2remaster;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.StatFs;
import android.util.Log;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Launcher. On the first run, and after an update that changes the game data,
 * it unpacks assets/zeq2data.zip into the app folder in shared storage
 * (Android/data/<package>/files/ZEQ2), the folder the engine uses as
 * fs_basepath and fs_homepath. Then it starts the game.
 *
 * Files the game writes there (zeq2config.cfg, qconsole.log...) are not in
 * the zip, so an update keeps them. Files an update no longer ships are
 * deleted: the list of what was unpacked stays next to the data.
 */
public class SetupActivity extends Activity {

    private static final String TAG = "ZEQ2";
    private static final String DATA_ZIP = "zeq2data.zip";
    private static final String DATA_ID = "zeq2data.id";
    private static final String MARKER = ".zeq2data.id";
    /** Paths unpacked last time, one per line, to remove what an update drops. */
    private static final String FILE_LIST = ".zeq2data.files";
    /** Room left after unpacking, for the config, logs and screenshots. */
    private static final long SPACE_MARGIN = 64L << 20;

    /** Intent extra with the folder that holds ZEQ2/, for ZEQ2Activity. */
    static final String EXTRA_DATA_ROOT = "dataRoot";

    private TextView status;
    private TextView detail;
    private ProgressBar progress;
    private File dataRoot;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File root = dataRoot();
        String[] id = readAssetId();
        if (id == null) {
            fail(getString(R.string.setup_missing));
            return;
        }
        dataRoot = root;
        if (id[0].equals(readMarker(root))) {
            startGame();
            return;
        }

        buildLayout();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        final long total = Math.max(1L, parseLong(id[1]));

        // An update overwrites the files it already has: only the rest needs room
        long needed = Math.max(0L, total - unpackedBytes(root)) + SPACE_MARGIN;
        long free = freeBytes(root);
        if (free >= 0 && free < needed) {
            fail(getString(R.string.setup_space, megabytes(needed), megabytes(free)));
            return;
        }
        new Thread(() -> unpack(root, id[0], total), "ZEQ2 setup").start();
    }

    /**
     * The folder that holds ZEQ2/. The game is told this exact folder
     * (fs_basepath and fs_homepath), so both always agree. Shared storage is
     * preferred, since it can be browsed over USB, but right after a boot it
     * may not be mounted yet: then the data goes to internal storage, and it
     * stays there afterwards instead of being unpacked twice.
     */
    private File dataRoot() {
        File external = getExternalFilesDir(null);
        File internal = getFilesDir();

        if (external != null && new File(external, "ZEQ2/" + MARKER).isFile() && writable(external)) {
            return external;
        }
        if (new File(internal, "ZEQ2/" + MARKER).isFile()) {
            return internal;
        }
        if (external != null && writable(external)) {
            return external;
        }
        Log.w(TAG, "Shared storage cannot be written, the data goes to internal storage");
        return internal;
    }

    /**
     * True when the game folder under root can be written. An uninstall that
     * kept the data leaves the old folder there, owned by the user id of the
     * old install: the new one can read its name but not write in it.
     */
    private static boolean writable(File root) {
        File probe = new File(new File(root, "ZEQ2"), ".zeq2write");
        try {
            mkdirs(probe.getParentFile());
            try (OutputStream os = new FileOutputStream(probe)) {
                os.write(0);
            }
            return probe.delete();
        } catch (IOException e) {
            return false;
        }
    }

    private void unpack(File root, String id, long total) {
        File gameDir = new File(root, "ZEQ2");
        long done = 0;
        int lastPercent = -1;
        byte[] buffer = new byte[1 << 16];
        List<String> names = new ArrayList<>();
        final String totalMb = megabytes(total);

        try (ZipInputStream zip = new ZipInputStream(
                new BufferedInputStream(getAssets().open(DATA_ZIP), 1 << 16))) {
            new File(gameDir, MARKER).delete();

            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                File out = new File(gameDir, entry.getName());
                // Never write outside the game folder
                if (!out.getCanonicalPath().startsWith(gameDir.getCanonicalPath() + File.separator)) {
                    throw new IOException("Bad entry " + entry.getName());
                }
                if (entry.isDirectory()) {
                    mkdirs(out);
                    continue;
                }
                mkdirs(out.getParentFile());
                names.add(entry.getName());
                try (OutputStream os = new FileOutputStream(out)) {
                    int n;
                    while ((n = zip.read(buffer)) > 0) {
                        os.write(buffer, 0, n);
                        done += n;
                        int percent = (int) (done * 100 / total);
                        if (percent != lastPercent) {
                            lastPercent = percent;
                            final int p = Math.min(percent, 100);
                            final String mb = megabytes(done);
                            runOnUiThread(() -> {
                                progress.setProgress(p);
                                detail.setText(getString(R.string.setup_progress, mb, totalMb));
                            });
                        }
                    }
                }
            }

            removeDropped(gameDir, names);
            writeFileList(gameDir, names);
            writeMarker(gameDir, id);
            Log.i(TAG, "Game data unpacked: " + done + " bytes in " + gameDir);
            runOnUiThread(this::startGame);
        } catch (IOException e) {
            Log.e(TAG, "Unpacking the game data failed", e);
            final String message = getString(R.string.setup_failed, e.getMessage());
            runOnUiThread(() -> fail(message));
        }
    }

    private void startGame() {
        Intent game = new Intent(this, ZEQ2Activity.class);
        // Keeps the adb test arguments: --es args "+map desert"
        Bundle extras = getIntent() != null ? getIntent().getExtras() : null;
        if (extras != null) {
            game.putExtras(extras);
        }
        game.putExtra(EXTRA_DATA_ROOT, dataRoot.getAbsolutePath());
        startActivity(game);
        finish();
        overridePendingTransition(0, 0);
    }

    private void buildLayout() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackgroundColor(Color.rgb(8, 10, 24));
        int pad = (int) (32 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(Color.WHITE);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        status = new TextView(this);
        status.setText(R.string.setup_unpacking);
        status.setTextColor(Color.rgb(170, 190, 230));
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, pad / 2, 0, pad / 2);
        layout.addView(status);

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        layout.addView(progress, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        detail = new TextView(this);
        detail.setTextColor(Color.rgb(120, 135, 170));
        detail.setTextSize(13);
        detail.setGravity(Gravity.CENTER);
        detail.setPadding(0, pad / 3, 0, 0);
        layout.addView(detail);

        setContentView(layout);
    }

    private void fail(String message) {
        if (status == null) {
            buildLayout();
        }
        status.setText(message);
        status.setTextColor(Color.rgb(255, 140, 120));
        progress.setVisibility(ProgressBar.GONE);
    }

    /**
     * Deletes the files of the last unpack that this one did not write: an
     * update that drops a file must not leave the old one for the game to
     * find. Files the game made itself were never on the list.
     */
    private static void removeDropped(File gameDir, List<String> names) {
        Set<String> now = new HashSet<>(names);
        for (String old : readFileList(gameDir)) {
            if (now.contains(old)) {
                continue;
            }
            File f = new File(gameDir, old);
            try {
                if (f.getCanonicalPath().startsWith(gameDir.getCanonicalPath() + File.separator) && f.isFile() && f.delete()) {
                    Log.i(TAG, "Removed " + old + ", dropped by this version");
                    // and the folders it leaves empty
                    File dir = f.getParentFile();
                    while (dir != null && !dir.equals(gameDir) && dir.delete()) {
                        dir = dir.getParentFile();
                    }
                }
            } catch (IOException ignored) {
                // keep going: one file that cannot go is not worth failing the setup
            }
        }
    }

    private static List<String> readFileList(File gameDir) {
        List<String> names = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new java.io.FileInputStream(new File(gameDir, FILE_LIST)), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (!line.isEmpty()) {
                    names.add(line);
                }
            }
        } catch (IOException ignored) {
            // first run, or data unpacked before the list existed
        }
        return names;
    }

    private static void writeFileList(File gameDir, List<String> names) throws IOException {
        try (OutputStream os = new FileOutputStream(new File(gameDir, FILE_LIST))) {
            for (String name : names) {
                os.write((name + "\n").getBytes(StandardCharsets.UTF_8));
            }
        }
    }

    /** Bytes of the files the last unpack wrote, still on disk. */
    private static long unpackedBytes(File root) {
        File gameDir = new File(root, "ZEQ2");
        long bytes = 0;
        for (String name : readFileList(gameDir)) {
            bytes += new File(gameDir, name).length();
        }
        return bytes;
    }

    /** Free bytes where the data goes, -1 if Android cannot tell. */
    private static long freeBytes(File root) {
        try {
            mkdirs(root);
            return new StatFs(root.getAbsolutePath()).getAvailableBytes();
        } catch (IOException | IllegalArgumentException e) {
            return -1;
        }
    }

    private static String megabytes(long bytes) {
        return String.valueOf((bytes + (1 << 19)) >> 20);
    }

    private String[] readAssetId() {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                getAssets().open(DATA_ID), StandardCharsets.UTF_8))) {
            String hash = reader.readLine();
            String bytes = reader.readLine();
            return hash != null && bytes != null ? new String[] { hash.trim(), bytes.trim() } : null;
        } catch (IOException e) {
            Log.e(TAG, "No " + DATA_ID + " in the APK", e);
            return null;
        }
    }

    private static String readMarker(File root) {
        File marker = new File(new File(root, "ZEQ2"), MARKER);
        try (InputStream in = new java.io.FileInputStream(marker)) {
            byte[] data = new byte[128];
            int n = in.read(data);
            return n > 0 ? new String(data, 0, n, StandardCharsets.UTF_8).trim() : null;
        } catch (IOException e) {
            return null;
        }
    }

    private static void writeMarker(File gameDir, String id) throws IOException {
        try (OutputStream os = new FileOutputStream(new File(gameDir, MARKER))) {
            os.write(id.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void mkdirs(File dir) throws IOException {
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("Cannot create " + dir);
        }
    }

    private static long parseLong(String s) {
        try {
            return Long.parseLong(s);
        } catch (NumberFormatException e) {
            return 1L;
        }
    }
}
