package io.github.francofuster.zeq2remaster;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
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
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Launcher. On the first run, and after an update that changes the game data,
 * it unpacks assets/zeq2data.zip into the app folder in shared storage
 * (Android/data/<package>/files/ZEQ2), the folder the engine uses as
 * fs_basepath and fs_homepath. Then it starts the game.
 *
 * Files the game writes there (zeq2config.cfg, qconsole.log...) are not in
 * the zip, so an update keeps them.
 */
public class SetupActivity extends Activity {

    private static final String TAG = "ZEQ2";
    private static final String DATA_ZIP = "zeq2data.zip";
    private static final String DATA_ID = "zeq2data.id";
    private static final String MARKER = ".zeq2data.id";

    private TextView status;
    private ProgressBar progress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File root = dataRoot();
        String[] id = readAssetId();
        if (id == null) {
            fail(getString(R.string.setup_missing));
            return;
        }
        if (id[0].equals(readMarker(root))) {
            startGame();
            return;
        }

        buildLayout();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        final long total = Math.max(1L, parseLong(id[1]));
        new Thread(() -> unpack(root, id[0], total), "ZEQ2 setup").start();
    }

    /** Same folder SDL_AndroidGetExternalStoragePath() gives the engine. */
    private File dataRoot() {
        File external = getExternalFilesDir(null);
        return external != null ? external : getFilesDir();
    }

    private void unpack(File root, String id, long total) {
        File gameDir = new File(root, "ZEQ2");
        long done = 0;
        int lastPercent = -1;
        byte[] buffer = new byte[1 << 16];

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
                try (OutputStream os = new FileOutputStream(out)) {
                    int n;
                    while ((n = zip.read(buffer)) > 0) {
                        os.write(buffer, 0, n);
                        done += n;
                        int percent = (int) (done * 100 / total);
                        if (percent != lastPercent) {
                            lastPercent = percent;
                            final int p = Math.min(percent, 100);
                            runOnUiThread(() -> progress.setProgress(p));
                        }
                    }
                }
            }

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
