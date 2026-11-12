package io.github.francofuster.zeq2remaster;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Hands a downloaded update APK to the Android installer. Since Android 7 a
 * file path cannot be passed to another app, only a content:// address that
 * this provider serves, read only, for the APK files in cache/update and
 * nothing else.
 */
public class ApkProvider extends ContentProvider {

    static final String UPDATE_DIR = "update";
    private static final String APK_TYPE = "application/vnd.android.package-archive";

    static Uri uriFor(Context context, File apk) {
        return new Uri.Builder().scheme("content")
                .authority(context.getPackageName() + ".apk")
                .appendPath(apk.getName())
                .build();
    }

    static File updateDir(Context context) {
        return new File(context.getCacheDir(), UPDATE_DIR);
    }

    private File fileFor(Uri uri) throws FileNotFoundException {
        String name = uri.getLastPathSegment();
        if (name == null || name.contains("/") || name.startsWith(".") || !name.endsWith(".apk")) {
            throw new FileNotFoundException(String.valueOf(uri));
        }
        File f = new File(updateDir(getContext()), name);
        if (!f.isFile()) {
            throw new FileNotFoundException(String.valueOf(uri));
        }
        return f;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) {
            throw new SecurityException("read only");
        }
        return ParcelFileDescriptor.open(fileFor(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return APK_TYPE;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        File f;
        try {
            f = fileFor(uri);
        } catch (FileNotFoundException e) {
            return null;
        }
        MatrixCursor c = new MatrixCursor(new String[] { OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE });
        c.addRow(new Object[] { f.getName(), f.length() });
        return c;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }
}
