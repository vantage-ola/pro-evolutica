package com.proevolutica.viewer;

import org.libsdl.app.SDLActivity;

public final class ViewerActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
