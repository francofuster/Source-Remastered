package io.github.francofuster.zeq2remaster;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.CRC32;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

/**
 * Updates from the GitHub releases of the project, without any UI.
 *
 * Each Android release (tag android-*) carries manifest.json, the APK and
 * zeq2data.zip. The data is never downloaded whole after the first time: the
 * central directory at the end of the zip lists every file with its CRC32,
 * and only the files that differ from the installed ones are fetched, as byte
 * ranges of the zip (GitHub answers range requests with 206).
 */
final class Updater {

    private static final String TAG = "ZEQ2";

    static final String DEFAULT_FEED =
            "https://api.github.com/repos/francofuster/Source-Remastered/releases?per_page=20";
    static final String TAG_PREFIX = "android-";
    static final String MANIFEST = "manifest.json";

    /** Unneeded bytes between two needed files below this are read and skipped, in one request. */
    private static final long MERGE_GAP = 256 * 1024;
    /** The index is saved every this many files, so an interrupted download keeps its work. */
    private static final int SAVE_EVERY = 25;

    private static final int CHECK_CONNECT_MS = 3000;
    private static final int CHECK_READ_MS = 4000;
    private static final int DOWNLOAD_CONNECT_MS = 10000;
    private static final int DOWNLOAD_READ_MS = 20000;

    private static final String PREFS = "updater";

    interface Progress {
        /** done and total in bytes; file and files count what is being written. */
        void update(long done, long total, int file, int files);
    }

    static final class Cancelled extends IOException {
        Cancelled() {
            super("cancelled");
        }
    }

    /** What the newest Android release offers, from its manifest.json. */
    static final class Release {
        String tag;
        String version;
        String notes;
        long versionCode;
        String apkName;
        String apkUrl;
        long apkBytes;
        String apkSha256;
        String dataName;
        String dataUrl;
        String dataId;
        long dataBytes;
        long dataUnpacked;
        int dataFiles;
        long dataNeeds;
    }

    /** One file of the remote zip, from its central directory. */
    static final class Entry {
        String name;
        int method;
        long crc;
        long csize;
        long usize;
        /** Where its local header starts, and where the next file starts. */
        long offset;
        long end;
    }

    private final String userAgent;
    private final SharedPreferences prefs;
    volatile boolean cancelled;

    Updater(Context context, String userAgent) {
        this.userAgent = userAgent;
        this.prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    // --- Which release -----------------------------------------------------

    /**
     * The newest Android release, or null when there is none or it has no
     * manifest (the releases before the updater). Short timeouts: with no
     * answer the game simply starts.
     */
    Release check(String feed) throws IOException, JSONException {
        JSONArray releases = new JSONArray(fetchFeed(feed));
        for (int i = 0; i < releases.length(); i++) {
            JSONObject rel = releases.getJSONObject(i);
            String tag = rel.optString("tag_name", "");
            if (rel.optBoolean("draft") || !tag.startsWith(TAG_PREFIX)) {
                continue;
            }
            // Only the newest one: an older release would roll the data back
            JSONArray assets = rel.optJSONArray("assets");
            String manifestUrl = assetUrl(assets, MANIFEST);
            if (manifestUrl == null) {
                Log.i(TAG, "Update: " + tag + " has no " + MANIFEST);
                return null;
            }
            JSONObject m = new JSONObject(readText(manifestUrl, CHECK_CONNECT_MS, CHECK_READ_MS));
            JSONObject apk = m.getJSONObject("apk");
            JSONObject data = m.getJSONObject("data");
            Release r = new Release();
            r.tag = tag;
            r.version = m.getString("version");
            r.versionCode = m.getLong("versionCode");
            r.notes = m.optString("notes", "");
            r.apkName = apk.getString("file");
            r.apkBytes = apk.getLong("bytes");
            r.apkSha256 = apk.getString("sha256").toLowerCase();
            r.apkUrl = assetUrl(assets, r.apkName);
            r.dataName = data.getString("file");
            r.dataId = data.getString("id");
            r.dataBytes = data.getLong("bytes");
            r.dataUnpacked = data.optLong("unpackedBytes", 0);
            r.dataFiles = data.optInt("files", 0);
            r.dataNeeds = data.optLong("needsVersionCode", r.versionCode);
            r.dataUrl = assetUrl(assets, r.dataName);
            if (r.apkUrl == null || r.dataUrl == null) {
                throw new IOException(tag + " lacks " + (r.apkUrl == null ? r.apkName : r.dataName));
            }
            Log.i(TAG, "Update: newest is " + tag + " (version " + r.version + ", code " + r.versionCode
                    + ", data " + r.dataId.substring(0, Math.min(12, r.dataId.length())) + ")");
            return r;
        }
        return null;
    }

    private static String assetUrl(JSONArray assets, String name) throws JSONException {
        if (assets == null) {
            return null;
        }
        for (int i = 0; i < assets.length(); i++) {
            JSONObject a = assets.getJSONObject(i);
            if (name.equals(a.optString("name"))) {
                return a.optString("browser_download_url", null);
            }
        }
        return null;
    }

    /**
     * The release list. The GitHub API allows 60 requests an hour per address
     * without an account; an answer that did not change (304 to the saved
     * ETag) does not count, so the last list is kept.
     */
    private String fetchFeed(String feed) throws IOException {
        HttpURLConnection c = open(feed, CHECK_CONNECT_MS, CHECK_READ_MS);
        c.setRequestProperty("Accept", "application/vnd.github+json");
        String etag = prefs.getString("etag:" + feed, null);
        String cached = prefs.getString("body:" + feed, null);
        if (etag != null && cached != null) {
            c.setRequestProperty("If-None-Match", etag);
        }
        try {
            int code = c.getResponseCode();
            if (code == HttpURLConnection.HTTP_NOT_MODIFIED && cached != null) {
                return cached;
            }
            if (code != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + code + " from the release list");
            }
            String body = readAll(c.getInputStream());
            String newTag = c.getHeaderField("ETag");
            SharedPreferences.Editor e = prefs.edit();
            if (newTag != null) {
                e.putString("etag:" + feed, newTag).putString("body:" + feed, body);
            } else {
                e.remove("etag:" + feed).remove("body:" + feed);
            }
            e.apply();
            return body;
        } finally {
            c.disconnect();
        }
    }

    // --- The remote zip ----------------------------------------------------

    /**
     * Every file of the release data, sorted by where it sits in the zip.
     * Reads only the end of the zip: the end record and the central
     * directory, about 140 KB for 1678 files.
     */
    List<Entry> readZipIndex(Release r) throws IOException {
        long size = r.dataBytes;
        int tailLen = (int) Math.min(size, 22 + 65535);
        byte[] tail = readRange(r, size - tailLen, tailLen);
        int eocd = -1;
        for (int p = tailLen - 22; p >= 0; p--) {
            if (u32(tail, p) == 0x06054b50L) {
                eocd = p;
                break;
            }
        }
        if (eocd < 0) {
            throw new IOException("No end of central directory in " + r.dataName);
        }
        int count = u16(tail, eocd + 10);
        long cdSize = u32(tail, eocd + 12);
        long cdOffset = u32(tail, eocd + 16);
        if (cdOffset + cdSize > size) {
            throw new IOException("Bad central directory in " + r.dataName);
        }
        byte[] cd;
        long tailStart = size - tailLen;
        if (cdOffset >= tailStart) {
            cd = new byte[(int) cdSize];
            System.arraycopy(tail, (int) (cdOffset - tailStart), cd, 0, (int) cdSize);
        } else {
            cd = readRange(r, cdOffset, (int) cdSize);
        }

        List<Entry> entries = new ArrayList<>(count);
        int p = 0;
        for (int i = 0; i < count; i++) {
            if (p + 46 > cd.length || u32(cd, p) != 0x02014b50L) {
                throw new IOException("Bad central directory entry " + i + " in " + r.dataName);
            }
            int nameLen = u16(cd, p + 28);
            int extraLen = u16(cd, p + 30);
            int commentLen = u16(cd, p + 32);
            Entry e = new Entry();
            e.method = u16(cd, p + 10);
            e.crc = u32(cd, p + 16);
            e.csize = u32(cd, p + 20);
            e.usize = u32(cd, p + 24);
            e.offset = u32(cd, p + 42);
            e.name = new String(cd, p + 46, nameLen, StandardCharsets.UTF_8);
            p += 46 + nameLen + extraLen + commentLen;
            if (!e.name.endsWith("/")) {
                entries.add(e);
            }
        }
        Collections.sort(entries, (a, b) -> Long.compare(a.offset, b.offset));
        for (int i = 0; i < entries.size(); i++) {
            entries.get(i).end = i + 1 < entries.size() ? entries.get(i + 1).offset : cdOffset;
        }
        return entries;
    }

    /**
     * The requests for the given files (sorted by offset): runs of files close
     * enough together share one, as {first index, last index}.
     */
    static List<int[]> runs(List<Entry> needed) {
        List<int[]> runs = new ArrayList<>();
        int i = 0;
        while (i < needed.size()) {
            int j = i;
            while (j + 1 < needed.size() && needed.get(j + 1).offset - needed.get(j).end <= MERGE_GAP) {
                j++;
            }
            runs.add(new int[] { i, j });
            i = j + 1;
        }
        return runs;
    }

    /** Bytes downloading the given files costs, small gaps between them included. */
    static long downloadBytes(List<Entry> needed) {
        long total = 0;
        for (int[] run : runs(needed)) {
            total += needed.get(run[1]).end - needed.get(run[0]).offset;
        }
        return total;
    }

    /** Callback after each file lands, to record it in the local index. */
    interface Landed {
        void file(Entry e, boolean saveNow) throws IOException;
    }

    /**
     * Downloads the given files (sorted by offset) into gameDir. Neighbours
     * share one range request. Each file is written next to its place,
     * checked against its CRC32 and only then moved over the old one, so a
     * cut connection never leaves a half file under the real name.
     */
    void download(Release r, List<Entry> needed, File gameDir, Landed landed, Progress progress)
            throws IOException {
        long total = downloadBytes(needed);
        long done = 0;
        int written = 0;
        String root = gameDir.getCanonicalPath() + File.separator;
        byte[] buffer = new byte[1 << 16];

        for (int[] run : runs(needed)) {
            // One request for a run of files close enough together
            int i = run[0];
            int j = run[1];
            long start = needed.get(i).offset;
            long end = needed.get(j).end;
            HttpURLConnection c = openRange(r, start, end - 1, DOWNLOAD_CONNECT_MS, DOWNLOAD_READ_MS);
            try (InputStream in = new BufferedInputStream(c.getInputStream(), 1 << 16)) {
                long pos = start;
                for (int k = i; k <= j; k++) {
                    Entry e = needed.get(k);
                    skipFully(in, e.offset - pos);
                    done += e.offset - pos;
                    pos = e.offset;

                    byte[] h = new byte[30];
                    readFully(in, h, 30);
                    if (u32(h, 0) != 0x04034b50L) {
                        throw new IOException("No local header for " + e.name);
                    }
                    int skip = u16(h, 26) + u16(h, 28);
                    skipFully(in, skip);
                    pos += 30 + skip;

                    File out = new File(gameDir, e.name);
                    if (!out.getCanonicalPath().startsWith(root)) {
                        throw new IOException("Bad entry " + e.name);
                    }
                    File dir = out.getParentFile();
                    if (dir != null && !dir.isDirectory() && !dir.mkdirs()) {
                        throw new IOException("Cannot create " + dir);
                    }
                    File part = new File(out.getPath() + ".zeq2part");
                    writeEntry(e, new Bounded(in, e.csize), part, buffer);
                    pos += e.csize;
                    if (!out.delete() && out.exists()) {
                        throw new IOException("Cannot replace " + e.name);
                    }
                    if (!part.renameTo(out)) {
                        throw new IOException("Cannot move " + e.name + " into place");
                    }
                    written++;
                    landed.file(e, written % SAVE_EVERY == 0);

                    // A data descriptor after it, if any, is skipped with the next file
                    done += pos - e.offset;
                    progress.update(Math.min(done, total), total, written, needed.size());
                    if (cancelled) {
                        throw new Cancelled();
                    }
                }
                done += end - pos;
            } finally {
                c.disconnect();
            }
        }
    }

    private void writeEntry(Entry e, InputStream raw, File part, byte[] buffer) throws IOException {
        CRC32 crc = new CRC32();
        long size = 0;
        Inflater inflater = e.method == 8 ? new Inflater(true) : null;
        if (e.method != 0 && e.method != 8) {
            throw new IOException("Unsupported compression " + e.method + " for " + e.name);
        }
        try (OutputStream os = new FileOutputStream(part)) {
            InputStream in = inflater != null ? new InflaterInputStream(raw, inflater, 1 << 16) : raw;
            int n;
            while ((n = in.read(buffer)) > 0) {
                os.write(buffer, 0, n);
                crc.update(buffer, 0, n);
                size += n;
                if (cancelled) {
                    throw new Cancelled();
                }
            }
            // Whatever the inflater did not need (normally nothing)
            while (raw.read(buffer) > 0) {
                // drain
            }
        } catch (IOException ex) {
            part.delete();
            throw ex;
        } finally {
            if (inflater != null) {
                inflater.end();
            }
        }
        if (size != e.usize || crc.getValue() != e.crc) {
            part.delete();
            throw new IOException("Damaged download of " + e.name);
        }
    }

    // --- The APK -----------------------------------------------------------

    /**
     * Downloads the release APK into dir and checks its SHA-256. A copy that
     * is already there and matches is kept (Android may restart the app
     * after the install permission is granted).
     */
    File downloadApk(Release r, File dir, Progress progress) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("Cannot create " + dir);
        }
        File out = new File(dir, r.apkName);
        if (out.isFile() && out.length() == r.apkBytes && r.apkSha256.equals(sha256(out))) {
            return out;
        }
        File[] old = dir.listFiles();
        if (old != null) {
            for (File f : old) {
                f.delete();
            }
        }
        MessageDigest digest = newSha256();
        HttpURLConnection c = open(r.apkUrl, DOWNLOAD_CONNECT_MS, DOWNLOAD_READ_MS);
        try {
            if (c.getResponseCode() != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + c.getResponseCode() + " for " + r.apkName);
            }
            long done = 0;
            byte[] buffer = new byte[1 << 16];
            try (InputStream in = c.getInputStream(); OutputStream os = new FileOutputStream(out)) {
                int n;
                while ((n = in.read(buffer)) > 0) {
                    os.write(buffer, 0, n);
                    digest.update(buffer, 0, n);
                    done += n;
                    progress.update(done, r.apkBytes, 1, 1);
                    if (cancelled) {
                        throw new Cancelled();
                    }
                }
            }
        } catch (IOException e) {
            out.delete();
            throw e;
        } finally {
            c.disconnect();
        }
        if (out.length() != r.apkBytes || !r.apkSha256.equals(hex(digest.digest()))) {
            out.delete();
            throw new IOException("The downloaded APK does not match the release");
        }
        return out;
    }

    static String sha256(File f) throws IOException {
        MessageDigest digest = newSha256();
        byte[] buffer = new byte[1 << 16];
        try (InputStream in = new FileInputStream(f)) {
            int n;
            while ((n = in.read(buffer)) > 0) {
                digest.update(buffer, 0, n);
            }
        }
        return hex(digest.digest());
    }

    // --- HTTP --------------------------------------------------------------

    private HttpURLConnection open(String url, int connectMs, int readMs) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(connectMs);
        c.setReadTimeout(readMs);
        c.setInstanceFollowRedirects(true);
        c.setRequestProperty("User-Agent", userAgent);
        return c;
    }

    /**
     * A range of the release zip. The total in Content-Range must be the size
     * the manifest gave: a release replaced in the middle of an update stops
     * it, instead of mixing two zips.
     */
    private HttpURLConnection openRange(Release r, long first, long last, int connectMs, int readMs)
            throws IOException {
        HttpURLConnection c = open(r.dataUrl, connectMs, readMs);
        c.setRequestProperty("Range", "bytes=" + first + "-" + last);
        c.setRequestProperty("Accept-Encoding", "identity");
        int code = c.getResponseCode();
        String range = c.getHeaderField("Content-Range");
        if (code != HttpURLConnection.HTTP_PARTIAL || range == null
                || !range.startsWith("bytes " + first + "-")
                || !range.endsWith("/" + r.dataBytes)) {
            c.disconnect();
            throw new IOException("The data server did not send the range (" + code + ", " + range + ")");
        }
        return c;
    }

    private byte[] readRange(Release r, long first, int length) throws IOException {
        HttpURLConnection c = openRange(r, first, first + length - 1, DOWNLOAD_CONNECT_MS, DOWNLOAD_READ_MS);
        try (InputStream in = new BufferedInputStream(c.getInputStream())) {
            byte[] data = new byte[length];
            readFully(in, data, length);
            return data;
        } finally {
            c.disconnect();
        }
    }

    private String readText(String url, int connectMs, int readMs) throws IOException {
        HttpURLConnection c = open(url, connectMs, readMs);
        try {
            if (c.getResponseCode() != HttpURLConnection.HTTP_OK) {
                throw new IOException("HTTP " + c.getResponseCode() + " for " + url);
            }
            return readAll(c.getInputStream());
        } finally {
            c.disconnect();
        }
    }

    private static String readAll(InputStream in) throws IOException {
        try (InputStream is = in) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[16384];
            int n;
            while ((n = is.read(buffer)) > 0) {
                out.write(buffer, 0, n);
            }
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }

    // --- Helpers -----------------------------------------------------------

    private static void readFully(InputStream in, byte[] b, int len) throws IOException {
        int off = 0;
        while (off < len) {
            int n = in.read(b, off, len - off);
            if (n < 0) {
                throw new EOFException();
            }
            off += n;
        }
    }

    private static void skipFully(InputStream in, long n) throws IOException {
        byte[] scratch = null;
        while (n > 0) {
            long s = in.skip(n);
            if (s <= 0) {
                if (scratch == null) {
                    scratch = new byte[8192];
                }
                int r = in.read(scratch, 0, (int) Math.min(scratch.length, n));
                if (r < 0) {
                    throw new EOFException();
                }
                s = r;
            }
            n -= s;
        }
    }

    private static int u16(byte[] b, int p) {
        return (b[p] & 0xff) | (b[p + 1] & 0xff) << 8;
    }

    private static long u32(byte[] b, int p) {
        return (u16(b, p) | (long) u16(b, p + 2) << 16) & 0xffffffffL;
    }

    private static MessageDigest newSha256() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }

    private static String hex(byte[] b) {
        StringBuilder s = new StringBuilder(b.length * 2);
        for (byte x : b) {
            s.append(Character.forDigit((x >> 4) & 15, 16)).append(Character.forDigit(x & 15, 16));
        }
        return s.toString();
    }

    /** At most n bytes of the stream, then end of stream; closing it does not close the stream. */
    private static final class Bounded extends FilterInputStream {
        private long left;

        Bounded(InputStream in, long n) {
            super(in);
            left = n;
        }

        @Override
        public int read() throws IOException {
            if (left <= 0) {
                return -1;
            }
            int b = in.read();
            if (b < 0) {
                throw new EOFException();
            }
            left--;
            return b;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            if (left <= 0) {
                return -1;
            }
            int n = in.read(b, off, (int) Math.min(len, left));
            if (n < 0) {
                throw new EOFException();
            }
            left -= n;
            return n;
        }

        @Override
        public long skip(long n) throws IOException {
            long s = in.skip(Math.min(n, left));
            left -= s;
            return s;
        }

        @Override
        public int available() throws IOException {
            return (int) Math.min(in.available(), left);
        }

        @Override
        public void close() {
            // the zip stream goes on with the next file
        }

        @Override
        public boolean markSupported() {
            return false;
        }
    }
}
