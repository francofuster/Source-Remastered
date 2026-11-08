package io.github.francofuster.zeq2remaster;

import android.content.Context;
import android.util.Log;
import android.view.SurfaceHolder;

import org.libsdl.app.SDLSurface;

/**
 * The game surface, rendered below the screen resolution: its buffer is
 * targetHeight pixels tall and the system compositor scales it up to the
 * view, at no cost to the game. The engine takes its size from the buffer
 * (SDL_GL_GetDrawableSize), so it renders fewer pixels.
 */
class ZEQ2Surface extends SDLSurface {

    private final int targetHeight;

    ZEQ2Surface(Context context, int targetHeight) {
        super(context);
        this.targetHeight = targetHeight;
    }

    /**
     * A SurfaceView with a fixed size measures itself at that size when the
     * layout lets it: once SDL adds its text view for the keyboard, the game
     * shrank to the top left corner and touches past it were lost. It always
     * takes the whole space instead; the compositor scales the buffer to it.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), MeasureSpec.getSize(heightMeasureSpec));
    }

    /**
     * Called on layout, before the surface exists, so the surface is created
     * at the reduced size and the engine never sees the full one.
     */
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        int shortSide = Math.min(w, h);
        if (targetHeight > 0 && shortSide > targetHeight) {
            float scale = (float) targetHeight / shortSide;
            int bw = Math.round(w * scale), bh = Math.round(h * scale);
            Log.v("ZEQ2", "Render size " + bw + "x" + bh + " for a view of " + w + "x" + h);
            getHolder().setFixedSize(bw, bh);
        } else {
            getHolder().setSizeFromLayout();
        }
    }

    /**
     * SDLSurface keeps the buffer size in mWidth and mHeight and divides the
     * touches by it, but touches come in view pixels: with a smaller buffer
     * they would go past 1. Normalize them by the view instead.
     */
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        super.surfaceChanged(holder, format, width, height);
        if (getWidth() > 0 && getHeight() > 0) {
            mWidth = getWidth();
            mHeight = getHeight();
        }
    }
}
