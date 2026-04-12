package com.gpubench.app;

import org.libsdl.app.SDLActivity;

public class DemoActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "gpu_demo"
        };
    }
}
