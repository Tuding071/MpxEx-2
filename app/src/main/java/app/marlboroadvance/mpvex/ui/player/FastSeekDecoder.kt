package app.marlboroadvance.mpvex.ui.player

import android.graphics.Bitmap
import android.graphics.BitmapFactory

class FastSeekDecoder {
    private external fun nativeInit(videoPath: String): Boolean
    private external fun nativeSeekTo(positionMs: Long): ByteArray?
    private external fun nativeGetDuration(): Long
    private external fun nativeRelease()
    
    private var isInitialized = false
    
    fun init(videoPath: String): Boolean {
        isInitialized = nativeInit(videoPath)
        return isInitialized
    }
    
    fun seekTo(positionMs: Long): Bitmap? {
        if (!isInitialized) return null
        val jpegData = nativeSeekTo(positionMs)
        
        // For now, return a dummy bitmap for testing
        // Remove this when native implementation returns real frames
        val dummyBitmap = Bitmap.createBitmap(160, 90, Bitmap.Config.ARGB_8888)
        dummyBitmap.eraseColor(0xFF303030.toInt()) // Dark gray
        
        return dummyBitmap
    }
    
    fun getDuration(): Long {
        return if (isInitialized) nativeGetDuration() else 0L
    }
    
    fun release() {
        if (isInitialized) {
            nativeRelease()
            isInitialized = false
        }
    }
    
    companion object {
        init {
            System.loadLibrary("fastseekdecoder")
        }
    }
}
