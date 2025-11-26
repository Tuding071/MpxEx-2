package app.marlboroadvance.mpvex.ui.player

import android.graphics.Bitmap

class FastSeekDecoder {
    private external fun nativeInit(videoPath: String): Boolean
    private external fun nativeSeekTo(positionMs: Long): Bitmap?
    private external fun nativeGetDuration(): Long
    private external fun nativeRelease()
    
    private var isInitialized = false
    
    fun init(videoPath: String): Boolean {
        isInitialized = nativeInit(videoPath)
        return isInitialized
    }
    
    fun seekTo(positionMs: Long): Bitmap? {
        if (!isInitialized) return null
        return try {
            nativeSeekTo(positionMs)
        } catch (e: Exception) {
            println("Native seek failed: ${e.message}")
            null
        }
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
