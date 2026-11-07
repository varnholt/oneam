package com.varnholt.oneam;

import android.content.Intent;
import android.net.Uri;
import android.database.Cursor;
import android.provider.OpenableColumns;
import java.nio.charset.StandardCharsets;
import org.libsdl.app.SDLActivity;

public final class OneamActivity extends SDLActivity {
    public byte[] documentName(String source) {
        try (Cursor cursor = getContentResolver().query(Uri.parse(source),
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null) return name.getBytes(StandardCharsets.UTF_8);
            }
        } catch (RuntimeException ignored) { /* A provider may no longer grant access. */ }
        return null;
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL3", "main" };
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        // The SDL picker uses ACTION_OPEN_DOCUMENT. Keep its read grant so recent
        // books remain readable after an app restart, including cloud providers.
        if (resultCode == RESULT_OK && data != null) {
            final Uri uri = data.getData();
            if (uri != null && "content".equals(uri.getScheme())) {
                final int flags = data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
                if (flags != 0) {
                    try { getContentResolver().takePersistableUriPermission(uri, flags); }
                    catch (SecurityException ignored) { /* Provider may offer a session-only grant. */ }
                }
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
}
