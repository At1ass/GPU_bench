package com.gpubench.app;

import org.libsdl.app.SDLActivity;

public class BenchmarkActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "gpu_benchmark"
        };
    }
}
