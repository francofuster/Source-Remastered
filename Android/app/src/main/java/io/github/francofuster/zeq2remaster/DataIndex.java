package io.github.francofuster.zeq2remaster;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.CRC32;

/**
 * The game files that came with the data, as name, size and CRC32: what an
 * update compares with the release zip to know which files changed, and what
 * it deletes when a version drops a file. Files the game writes itself
 * (config, logs, screenshots) are never in it.
 */
final class DataIndex {

    static final String FILE = ".zeq2data.index";
    /** The names only list of 1.4-alpha2 and before, migrated once. */
    static final String OLD_LIST = ".zeq2data.files";

    interface Progress {
        void update(long done, long total);
    }

    /** name -> {crc32, size} */
    final Map<String, long[]> files = new LinkedHashMap<>();

    static DataIndex load(File gameDir, Progress progress) throws IOException {
        DataIndex index = new DataIndex();
        File file = new File(gameDir, FILE);
        if (file.isFile()) {
            try (BufferedReader in = reader(file)) {
                String line;
                while ((line = in.readLine()) != null) {
                    String[] p = line.split("\t", 3);
                    if (p.length == 3) {
                        try {
                            index.files.put(p[2], new long[] { Long.parseLong(p[0], 16), Long.parseLong(p[1]) });
                        } catch (NumberFormatException ignored) {
                            // a bad line only costs downloading that file again
                        }
                    }
                }
            }
            return index;
        }

        // Data unpacked by an older version: read what is there once
        List<String> names = readOldList(gameDir);
        long total = 0;
        for (String name : names) {
            total += new File(gameDir, name).length();
        }
        long done = 0;
        byte[] buffer = new byte[1 << 16];
        for (String name : names) {
            File f = new File(gameDir, name);
            if (!f.isFile()) {
                continue;
            }
            CRC32 crc = new CRC32();
            try (InputStream in = new FileInputStream(f)) {
                int n;
                while ((n = in.read(buffer)) > 0) {
                    crc.update(buffer, 0, n);
                    done += n;
                    progress.update(done, total);
                }
            }
            index.files.put(name, new long[] { crc.getValue(), f.length() });
        }
        if (!names.isEmpty()) {
            index.save(gameDir);
        }
        return index;
    }

    void put(String name, long crc, long size) {
        files.put(name, new long[] { crc, size });
    }

    /** True when the file on disk is the one the index says, as far as its size tells. */
    boolean matches(File gameDir, String name, long crc, long size) {
        long[] have = files.get(name);
        return have != null && have[0] == crc && have[1] == size && new File(gameDir, name).length() == size;
    }

    /** Written next to it first, so a cut never leaves half an index. */
    void save(File gameDir) throws IOException {
        File tmp = new File(gameDir, FILE + ".tmp");
        try (OutputStream os = new FileOutputStream(tmp)) {
            StringBuilder s = new StringBuilder(files.size() * 48);
            for (Map.Entry<String, long[]> e : files.entrySet()) {
                s.append(Long.toHexString(e.getValue()[0])).append('\t')
                        .append(e.getValue()[1]).append('\t')
                        .append(e.getKey()).append('\n');
            }
            os.write(s.toString().getBytes(StandardCharsets.UTF_8));
        }
        File file = new File(gameDir, FILE);
        if (!tmp.renameTo(file)) {
            file.delete();
            if (!tmp.renameTo(file)) {
                throw new IOException("Cannot write " + file);
            }
        }
        // the old list is not needed any more
        new File(gameDir, OLD_LIST).delete();
    }

    private static List<String> readOldList(File gameDir) {
        List<String> names = new ArrayList<>();
        File list = new File(gameDir, OLD_LIST);
        if (!list.isFile()) {
            return names;
        }
        try (BufferedReader in = reader(list)) {
            String line;
            while ((line = in.readLine()) != null) {
                if (!line.isEmpty()) {
                    names.add(line);
                }
            }
        } catch (IOException ignored) {
            // nothing to migrate
        }
        return names;
    }

    private static BufferedReader reader(File f) throws IOException {
        return new BufferedReader(new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8));
    }
}
