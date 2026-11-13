package io.github.francofuster.zeq2remaster;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StatFs;
import android.provider.Settings;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Launcher. Gets the game data in place, then starts the game.
 *
 * The release APK carries no data: it asks GitHub for the newest Android
 * release (Updater), offers a newer APK when there is one, and downloads the
 * data the first time and then only the files that changed. A development
 * APK built with the data inside (assets/zeq2data.zip) unpacks it as before
 * and does not look for updates.
 *
 * The data goes into the app folder in shared storage
 * (Android/data/<package>/files/ZEQ2), the folder the engine uses as
 * fs_basepath and fs_homepath. Files the game writes there (zeq2config.cfg,
 * qconsole.log...) are not part of the data, so updates keep them. Files a
 * version no longer ships are deleted, from the index kept next to the data.
 */
public class SetupActivity extends Activity {

    private static final String TAG = "ZEQ2";
    private static final String DATA_ZIP = "zeq2data.zip";
    private static final String DATA_ID = "zeq2data.id";
    private static final String MARKER = ".zeq2data.id";
    /** Room left after unpacking, for the config, logs and screenshots. */
    private static final long SPACE_MARGIN = 64L << 20;

    /** Intent extra with the folder that holds ZEQ2/, for ZEQ2Activity. */
    static final String EXTRA_DATA_ROOT = "dataRoot";
    /**
     * Intent extra for tests: a release list to use instead of GitHub, only on
     * this device or the emulator host (adb reverse, 10.0.2.2):
     *   --es updateFeed http://localhost:8000/releases
     */
    static final String EXTRA_UPDATE_FEED = "updateFeed";

    private static final int REQ_PERMISSION = 1;
    private static final int REQ_INSTALL = 2;
    private static final Pattern UPDATE_CHECK =
            Pattern.compile("^\\s*seta?\\s+cl_updateCheck\\s+\"?(\\d+)\"?", Pattern.MULTILINE);

    private TextView status;
    private TextView detail;
    private ProgressBar progress;
    private Button primary;
    private Button secondary;
    private File dataRoot;

    private Updater updater;
    private Updater.Release release;
    private long installedCode;
    private boolean hadData;
    private boolean apkDeclined;
    private File pendingApk;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        dataRoot = dataRoot();
        String[] bundled = readAssetId();
        String feed = feedOverride();
        if (bundled != null && feed == null) {
            bundledData(bundled);
            return;
        }
        if (feed == null) {
            feed = Updater.DEFAULT_FEED;
        }

        hadData = readMarker(dataRoot) != null;
        if (hadData && !updateCheckEnabled()) {
            startGame();
            return;
        }
        installedCode = installedVersionCode();
        updater = new Updater(this, "ZEQ2-Lite-Mobile/" + versionName());

        buildLayout();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        showWork(getString(R.string.update_checking));
        final String url = feed;
        // the files of each release, for when the release list cannot be read
        final String downloads = url.equals(Updater.DEFAULT_FEED) ? Updater.DEFAULT_DOWNLOADS
                : Uri.parse(url).buildUpon().path("/files/").clearQuery().build().toString();
        final String ownTag = Updater.TAG_PREFIX + versionName();
        final boolean patient = !hadData;
        worker(() -> {
            Updater.Release found = null;
            String failure = null;
            try {
                found = updater.check(url, downloads, ownTag, patient);
            } catch (Exception e) {
                Log.w(TAG, "Update check failed: " + e);
                failure = Updater.describe(e);
            }
            final Updater.Release r = found;
            final String why = failure;
            runOnUiThread(() -> checked(r, why));
        });
    }

    @Override
    protected void onDestroy() {
        if (updater != null) {
            updater.cancelled = true;
        }
        super.onDestroy();
    }

    // --- Update flow -------------------------------------------------------

    /** why: what failed, null when the release list was read. */
    private void checked(Updater.Release r, String why) {
        if (r == null || r.versionCode < installedCode) {
            // Nothing newer, or no answer: play with what is there
            if (hadData) {
                startGame();
            } else {
                failWith(getString(why == null ? R.string.update_no_release : R.string.update_offline_first),
                        this::recreate, null);
                if (why != null) {
                    detail.setText(why);
                }
            }
            return;
        }
        release = r;
        decide();
    }

    /** What to do with the release: the APK first, then the data, then play. */
    private void decide() {
        String marker = readMarker(dataRoot);
        boolean apkNewer = release.versionCode > installedCode;
        boolean dataDiffers = marker == null || !release.dataId.equals(marker);
        boolean dataFits = release.dataNeeds <= installedCode;

        if (apkNewer && !apkDeclined) {
            askApk();
        } else if (dataDiffers && dataFits) {
            planData();
        } else if (marker != null) {
            startGame();
        } else {
            failWith(getString(R.string.update_needs_apk), () -> {
                apkDeclined = false;
                decide();
            }, null);
        }
    }

    private void askApk() {
        String message = notes() + getString(R.string.update_apk_body, size(release.apkBytes));
        ask(getString(R.string.update_apk_title, release.version), message,
                this::downloadApk,
                () -> {
                    apkDeclined = true;
                    decide();
                });
    }

    private void downloadApk() {
        showWork(getString(R.string.update_downloading_apk));
        worker(() -> {
            try {
                File apk = updater.downloadApk(release, ApkProvider.updateDir(this),
                        (done, total, file, files) -> showProgress(done, total, 0, 0));
                runOnUiThread(() -> install(apk));
            } catch (Updater.Cancelled ignored) {
                // the activity is gone
            } catch (IOException e) {
                Log.e(TAG, "Downloading the APK failed", e);
                final String message = getString(R.string.update_failed, e.getMessage());
                runOnUiThread(() -> failWith(message, this::downloadApk, () -> {
                    apkDeclined = true;
                    decide();
                }));
            }
        });
    }

    private void install(File apk) {
        pendingApk = apk;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !getPackageManager().canRequestPackageInstalls()) {
            ask(getString(R.string.update_permission_title), getString(R.string.update_permission),
                    getString(R.string.update_open_settings), getString(R.string.update_later),
                    () -> {
                        Intent settings = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                Uri.parse("package:" + getPackageName()));
                        try {
                            startActivityForResult(settings, REQ_PERMISSION);
                        } catch (ActivityNotFoundException e) {
                            launchInstaller();
                        }
                    },
                    () -> {
                        apkDeclined = true;
                        decide();
                    });
            return;
        }
        launchInstaller();
    }

    private void launchInstaller() {
        Intent install = new Intent(Intent.ACTION_VIEW);
        install.setDataAndType(ApkProvider.uriFor(this, pendingApk), "application/vnd.android.package-archive");
        install.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            showWork(getString(R.string.update_installing));
            startActivityForResult(install, REQ_INSTALL);
        } catch (ActivityNotFoundException e) {
            failWith(getString(R.string.update_failed, e.getMessage()), this::launchInstaller, () -> {
                apkDeclined = true;
                decide();
            });
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (release == null) {
            return;
        }
        if (requestCode == REQ_PERMISSION) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || getPackageManager().canRequestPackageInstalls()) {
                launchInstaller();
            } else {
                apkDeclined = true;
                decide();
            }
        } else if (requestCode == REQ_INSTALL) {
            // Back here means it was not installed: an install restarts the app
            apkDeclined = true;
            decide();
        }
    }

    /** Compares the release zip with the installed files, then asks or starts. */
    private void planData() {
        showWork(getString(R.string.update_checking_files));
        worker(() -> {
            try {
                File gameDir = new File(dataRoot, "ZEQ2");
                mkdirs(gameDir);
                DataIndex index = DataIndex.load(gameDir, (done, total) -> showProgress(done, total, 0, 0));
                List<Updater.Entry> entries = updater.readZipIndex(release);
                List<Updater.Entry> needed = new ArrayList<>();
                Set<String> names = new HashSet<>();
                for (Updater.Entry e : entries) {
                    names.add(e.name);
                    if (!index.matches(gameDir, e.name, e.crc, e.usize)) {
                        needed.add(e);
                    }
                }
                List<String> dropped = new ArrayList<>();
                for (String name : index.files.keySet()) {
                    if (!names.contains(name)) {
                        dropped.add(name);
                    }
                }
                Log.i(TAG, "Update: " + needed.size() + " of " + entries.size() + " files to download ("
                        + Updater.downloadBytes(needed) + " bytes), " + dropped.size() + " to remove");
                runOnUiThread(() -> planned(gameDir, index, needed, dropped));
            } catch (Updater.Cancelled ignored) {
                // the activity is gone
            } catch (IOException e) {
                Log.e(TAG, "Reading the release data failed", e);
                failLater(e);
            }
        });
    }

    private void planned(File gameDir, DataIndex index, List<Updater.Entry> needed, List<String> dropped) {
        if (needed.isEmpty()) {
            worker(() -> finishData(gameDir, index, dropped));
            return;
        }
        if (!hadData) {
            downloadData(gameDir, index, needed, dropped);
            return;
        }
        String message = notes() + getResources().getQuantityString(R.plurals.update_data_body,
                needed.size(), needed.size(), size(Updater.downloadBytes(needed)));
        ask(getString(R.string.update_data_title, release.version), message,
                () -> downloadData(gameDir, index, needed, dropped),
                this::startGame);
    }

    private void downloadData(File gameDir, DataIndex index, List<Updater.Entry> needed, List<String> dropped) {
        long need = SPACE_MARGIN;
        for (Updater.Entry e : needed) {
            need += Math.max(0L, e.usize - new File(gameDir, e.name).length());
        }
        long free = freeBytes(dataRoot);
        if (free >= 0 && free < need) {
            failWith(getString(R.string.setup_space, megabytes(need), megabytes(free)), this::planData,
                    hadData ? this::startGame : null);
            return;
        }
        showWork(getString(hadData ? R.string.update_downloading_data : R.string.update_downloading_first));
        worker(() -> {
            try {
                updater.download(release, needed, gameDir,
                        (e, saveNow) -> {
                            index.put(e.name, e.crc, e.usize);
                            if (saveNow) {
                                index.save(gameDir);
                            }
                        },
                        (done, total, file, files) -> showProgress(done, total, file, files));
                finishData(gameDir, index, dropped);
            } catch (Updater.Cancelled ignored) {
                saveQuietly(index, gameDir);
            } catch (IOException e) {
                Log.e(TAG, "Downloading the game data failed", e);
                saveQuietly(index, gameDir);
                failLater(e);
            }
        });
    }

    /** On the worker: drop what this version no longer ships, then mark the data as this release. */
    private void finishData(File gameDir, DataIndex index, List<String> dropped) {
        try {
            for (String name : dropped) {
                removeFile(gameDir, name);
                index.files.remove(name);
            }
            index.save(gameDir);
            writeMarker(gameDir, release.dataId);
            Log.i(TAG, "Game data updated to " + release.tag);
            runOnUiThread(this::startGame);
        } catch (IOException e) {
            Log.e(TAG, "Finishing the update failed", e);
            failLater(e);
        }
    }

    private void failLater(IOException e) {
        final String message = getString(R.string.update_failed, e.getMessage());
        runOnUiThread(() -> failWith(message, this::planData, hadData ? this::startGame : null));
    }

    private static void saveQuietly(DataIndex index, File gameDir) {
        try {
            index.save(gameDir);
        } catch (IOException ignored) {
            // the next update downloads those files again
        }
    }

    private String notes() {
        return release.notes == null || release.notes.trim().isEmpty() ? "" : release.notes.trim() + "\n\n";
    }

    // --- Data inside the APK (development builds) --------------------------

    private void bundledData(String[] id) {
        if (id[0].equals(readMarker(dataRoot))) {
            startGame();
            return;
        }
        buildLayout();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        status.setText(R.string.setup_unpacking);
        final long total = Math.max(1L, parseLong(id[1]));

        // An update overwrites the files it already has: only the rest needs room
        long needed = Math.max(0L, total - unpackedBytes(dataRoot)) + SPACE_MARGIN;
        long free = freeBytes(dataRoot);
        if (free >= 0 && free < needed) {
            failWith(getString(R.string.setup_space, megabytes(needed), megabytes(free)), null, null);
            return;
        }
        new Thread(() -> unpack(dataRoot, id[0], total), "ZEQ2 setup").start();
    }

    private void unpack(File root, String id, long total) {
        File gameDir = new File(root, "ZEQ2");
        long done = 0;
        int lastPercent = -1;
        byte[] buffer = new byte[1 << 16];
        DataIndex index = new DataIndex();
        final String totalMb = megabytes(total);

        try (ZipInputStream zip = new ZipInputStream(
                new BufferedInputStream(getAssets().open(DATA_ZIP), 1 << 16))) {
            List<String> before = previousNames(gameDir);
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
                CRC32 crc = new CRC32();
                long size = 0;
                try (OutputStream os = new FileOutputStream(out)) {
                    int n;
                    while ((n = zip.read(buffer)) > 0) {
                        os.write(buffer, 0, n);
                        crc.update(buffer, 0, n);
                        size += n;
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
                index.put(entry.getName(), crc.getValue(), size);
            }

            for (String old : before) {
                if (!index.files.containsKey(old)) {
                    removeFile(gameDir, old);
                }
            }
            index.save(gameDir);
            writeMarker(gameDir, id);
            Log.i(TAG, "Game data unpacked: " + done + " bytes in " + gameDir);
            runOnUiThread(this::startGame);
        } catch (IOException e) {
            Log.e(TAG, "Unpacking the game data failed", e);
            final String message = getString(R.string.setup_failed, e.getMessage());
            runOnUiThread(() -> failWith(message, null, null));
        }
    }

    // --- Shared --------------------------------------------------------------

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

    private void startGame() {
        if (isFinishing()) {
            return;
        }
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

    /** cl_updateCheck 0 in the saved config turns the check at start off. */
    private boolean updateCheckEnabled() {
        File config = new File(dataRoot, "ZEQ2/zeq2config.cfg");
        try (BufferedReader in = new BufferedReader(new FileReader(config))) {
            StringBuilder text = new StringBuilder();
            String line;
            while ((line = in.readLine()) != null) {
                text.append(line).append('\n');
            }
            Matcher m = UPDATE_CHECK.matcher(text);
            return !m.find() || !"0".equals(m.group(1));
        } catch (IOException e) {
            return true;
        }
    }

    /** The test release list from the intent, only when it points at this device or the emulator host. */
    private String feedOverride() {
        Intent intent = getIntent();
        String feed = intent != null ? intent.getStringExtra(EXTRA_UPDATE_FEED) : null;
        if (feed == null) {
            return null;
        }
        Uri uri = Uri.parse(feed);
        String host = uri.getHost();
        if (host != null && (host.equals("localhost") || host.equals("127.0.0.1") || host.equals("10.0.2.2"))) {
            Log.i(TAG, "Update: test release list " + feed);
            return feed;
        }
        Log.w(TAG, "Update: ignoring the release list " + feed + " (only localhost or 10.0.2.2)");
        return null;
    }

    private long installedVersionCode() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? info.getLongVersionCode() : info.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            return 0;
        }
    }

    private String versionName() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            return "?";
        }
    }

    private static void worker(Runnable r) {
        new Thread(r, "ZEQ2 setup").start();
    }

    // --- Screen --------------------------------------------------------------

    private void buildLayout() {
        if (status != null) {
            return;
        }
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

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.CENTER);
        buttons.setPadding(0, pad / 2, 0, 0);
        secondary = new Button(this);
        primary = new Button(this);
        buttons.addView(secondary);
        buttons.addView(primary);
        secondary.setVisibility(View.GONE);
        primary.setVisibility(View.GONE);
        layout.addView(buttons);

        setContentView(layout);
    }

    /** Work under way: the status line and a bar that fills as it goes. */
    private void showWork(String text) {
        buildLayout();
        status.setText(text);
        status.setTextColor(Color.rgb(170, 190, 230));
        progress.setVisibility(View.VISIBLE);
        progress.setIndeterminate(true);
        detail.setText("");
        primary.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
    }

    private long lastProgressUi;

    /** From the worker; at most every 100 ms. */
    private void showProgress(long done, long total, int file, int files) {
        long now = System.currentTimeMillis();
        if (now - lastProgressUi < 100 && done < total) {
            return;
        }
        lastProgressUi = now;
        final int percent = total > 0 ? (int) Math.min(100, done * 100 / total) : 0;
        final String text = files > 1
                ? getString(R.string.update_progress_files, size(done), size(total), file, files)
                : getString(R.string.update_progress, size(done), size(total));
        runOnUiThread(() -> {
            if (progress == null) {
                return;
            }
            progress.setIndeterminate(false);
            progress.setProgress(percent);
            detail.setText(text);
        });
    }

    /** An error, with a button to try again and, when the game can run as it is, one to play. */
    private void failWith(String message, Runnable retry, Runnable play) {
        buildLayout();
        status.setText(message);
        status.setTextColor(Color.rgb(255, 140, 120));
        progress.setVisibility(View.GONE);
        detail.setText("");
        setButton(primary, retry != null ? getString(R.string.update_retry) : null, retry);
        setButton(secondary, play != null ? getString(R.string.update_play) : null, play);
    }

    private static void setButton(Button b, String label, Runnable action) {
        if (label == null) {
            b.setVisibility(View.GONE);
            return;
        }
        b.setText(label);
        b.setVisibility(View.VISIBLE);
        b.setOnClickListener(v -> action.run());
    }

    private void ask(String title, String message, Runnable yes, Runnable no) {
        ask(title, message, getString(R.string.update_now), getString(R.string.update_later), yes, no);
    }

    private void ask(String title, String message, String yesLabel, String noLabel, Runnable yes, Runnable no) {
        if (isFinishing()) {
            return;
        }
        progress.setVisibility(View.GONE);
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle(title)
                .setMessage(message)
                .setCancelable(false)
                .setPositiveButton(yesLabel, (d, w) -> yes.run())
                .setNegativeButton(noLabel, (d, w) -> no.run())
                .show();
    }

    /** "3 KB", "7.5 MB", "219 MB": KB below 1 MB, one decimal below 10 MB. */
    private static String size(long bytes) {
        if (bytes < 1048576) {
            return String.format(Locale.getDefault(), "%d KB", Math.max(1L, (bytes + 1023) / 1024));
        }
        double mb = bytes / 1048576.0;
        return mb < 10 ? String.format(Locale.getDefault(), "%.1f MB", mb) : String.format(Locale.getDefault(), "%d MB", Math.round(mb));
    }

    // --- Files ---------------------------------------------------------------

    private static void removeFile(File gameDir, String name) {
        File f = new File(gameDir, name);
        try {
            if (f.getCanonicalPath().startsWith(gameDir.getCanonicalPath() + File.separator) && f.isFile() && f.delete()) {
                Log.i(TAG, "Removed " + name + ", dropped by this version");
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

    /** Names of the data files on disk, from the index or the older list. */
    private static List<String> previousNames(File gameDir) {
        List<String> names = new ArrayList<>();
        for (String list : new String[] { DataIndex.FILE, DataIndex.OLD_LIST }) {
            File f = new File(gameDir, list);
            if (!f.isFile()) {
                continue;
            }
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                    new java.io.FileInputStream(f), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    if (line.isEmpty()) {
                        continue;
                    }
                    String[] p = line.split("\t", 3);
                    names.add(p.length == 3 ? p[2] : line);
                }
            } catch (IOException ignored) {
                // nothing known
            }
            break;
        }
        return names;
    }

    /** Bytes of the files the last unpack wrote, still on disk. */
    private static long unpackedBytes(File root) {
        File gameDir = new File(root, "ZEQ2");
        long bytes = 0;
        for (String name : previousNames(gameDir)) {
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

    /** The data inside the APK (development builds), or null for the release APK. */
    private String[] readAssetId() {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                getAssets().open(DATA_ID), StandardCharsets.UTF_8))) {
            String hash = reader.readLine();
            String bytes = reader.readLine();
            return hash != null && bytes != null ? new String[] { hash.trim(), bytes.trim() } : null;
        } catch (IOException e) {
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
