#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include <android/log.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#define LOG_TAG "FastSeekDecoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class FastSeekDecoder {
private:
    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    bool initialized = false;
    int64_t durationUs = 0;
    
public:
    FastSeekDecoder() {
        LOGI("FastSeekDecoder created");
    }
    
    ~FastSeekDecoder() {
        release();
    }
    
    bool init(const char* videoPath) {
        LOGI("Initializing with path: %s", videoPath);
        
        // Release if already initialized
        release();
        
        // Create media extractor
        extractor = AMediaExtractor_new();
        if (!extractor) {
            LOGE("Failed to create media extractor");
            return false;
        }
        
        // Try to set data source - simplified approach
        media_status_t status = AMediaExtractor_setDataSource(extractor, videoPath);
        if (status != AMEDIA_OK) {
            LOGW("setDataSource failed: %d, trying alternative approach", status);
            
            // For local files, try with file:// prefix
            std::string filePath = "file://";
            filePath += videoPath;
            status = AMediaExtractor_setDataSource(extractor, filePath.c_str());
            
            if (status != AMEDIA_OK) {
                LOGE("All setDataSource attempts failed");
                release();
                return false;
            }
        }
        
        // Find video track
        size_t numTracks = AMediaExtractor_getTrackCount(extractor);
        LOGI("Found %zu tracks", numTracks);
        
        for (size_t i = 0; i < numTracks; i++) {
            AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, i);
            if (!format) continue;
            
            const char* mime;
            if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
                LOGI("Track %zu: %s", i, mime);
                if (strncmp(mime, "video/", 6) == 0) {
                    // Get video properties
                    AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &durationUs);
                    LOGI("Video duration: %lld us", (long long)durationUs);
                    
                    // Select this track
                    AMediaExtractor_selectTrack(extractor, i);
                    
                    // Create decoder
                    codec = AMediaCodec_createDecoderByType(mime);
                    if (!codec) {
                        LOGE("Failed to create codec for %s", mime);
                        AMediaFormat_delete(format);
                        release();
                        return false;
                    }
                    
                    // Configure codec for low resolution
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_WIDTH, 426);
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_HEIGHT, 240);
                    
                    // Configure and start codec
                    status = AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
                    if (status != AMEDIA_OK) {
                        LOGE("Failed to configure codec: %d", status);
                        AMediaFormat_delete(format);
                        release();
                        return false;
                    }
                    
                    status = AMediaCodec_start(codec);
                    if (status != AMEDIA_OK) {
                        LOGE("Failed to start codec: %d", status);
                        AMediaFormat_delete(format);
                        release();
                        return false;
                    }
                    
                    AMediaFormat_delete(format);
                    initialized = true;
                    LOGI("FastSeekDecoder initialized successfully");
                    return true;
                }
            }
            AMediaFormat_delete(format);
        }
        
        LOGE("No suitable video track found");
        release();
        return false;
    }
    
    jobject seekToFrame(JNIEnv* env, jlong positionMs) {
        if (!initialized) {
            LOGE("Decoder not initialized");
            return createTestBitmap(env);
        }
        
        int64_t positionUs = positionMs * 1000; // Convert ms to microseconds
        LOGI("Seeking to: %lld ms", (long long)positionMs);
        
        // Seek to position
        media_status_t status = AMediaExtractor_seekTo(extractor, positionUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
        if (status != AMEDIA_OK) {
            LOGW("Seek failed: %d, returning test bitmap", status);
            return createTestBitmap(env);
        }
        
        // For now, return a test bitmap
        // In a full implementation, you would decode actual frames here
        return createTestBitmap(env);
    }
    
    jobject createTestBitmap(JNIEnv* env) {
        // Create a test bitmap with color gradient
        const int width = 426;
        const int height = 240;
        
        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        if (!bitmapClass) {
            LOGE("Failed to find Bitmap class");
            return nullptr;
        }
        
        jmethodID createBitmapMethod = env->GetStaticMethodID(
            bitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
        if (!createBitmapMethod) {
            LOGE("Failed to find createBitmap method");
            return nullptr;
        }
        
        jclass configClass = env->FindClass("android/graphics/Bitmap$Config");
        if (!configClass) {
            LOGE("Failed to find Bitmap$Config class");
            return nullptr;
        }
        
        jfieldID argb8888Field = env->GetStaticFieldID(configClass, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
        if (!argb8888Field) {
            LOGE("Failed to find ARGB_8888 field");
            return nullptr;
        }
        
        jobject config = env->GetStaticObjectField(configClass, argb8888Field);
        if (!config) {
            LOGE("Failed to get ARGB_8888 config");
            return nullptr;
        }
        
        // Create bitmap
        jobject bitmap = env->CallStaticObjectMethod(bitmapClass, createBitmapMethod, width, height, config);
        
        if (bitmap) {
            // Fill with test pattern
            AndroidBitmapInfo info;
            if (AndroidBitmap_getInfo(env, bitmap, &info) == ANDROID_BITMAP_RESULT_SUCCESS) {
                void* pixels;
                if (AndroidBitmap_lockPixels(env, bitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS) {
                    // Create gradient pattern
                    uint32_t* pixelArray = static_cast<uint32_t*>(pixels);
                    for (int y = 0; y < info.height; y++) {
                        for (int x = 0; x < info.width; x++) {
                            uint8_t r = (x * 255) / info.width;
                            uint8_t g = (y * 255) / info.height;
                            uint8_t b = 128;
                            // Create color based on position for visual feedback
                            pixelArray[y * info.width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                        }
                    }
                    AndroidBitmap_unlockPixels(env, bitmap);
                    LOGI("Test bitmap created successfully");
                }
            }
        }
        
        return bitmap;
    }
    
    jlong getDuration() {
        return durationUs / 1000; // Convert to milliseconds
    }
    
    void release() {
        LOGI("Releasing FastSeekDecoder");
        
        if (codec) {
            AMediaCodec_stop(codec);
            AMediaCodec_delete(codec);
            codec = nullptr;
        }
        
        if (extractor) {
            AMediaExtractor_delete(extractor);
            extractor = nullptr;
        }
        
        initialized = false;
        durationUs = 0;
    }
};

// Global instance
FastSeekDecoder g_decoder;

extern "C" JNIEXPORT jboolean JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeInit(
    JNIEnv* env,
    jobject thiz,
    jstring videoPath) {
    
    const char* path = env->GetStringUTFChars(videoPath, nullptr);
    bool result = g_decoder.init(path);
    env->ReleaseStringUTFChars(videoPath, path);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeSeekTo(
    JNIEnv* env,
    jobject thiz,
    jlong positionMs) {
    
    return g_decoder.seekToFrame(env, positionMs);
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeGetDuration(
    JNIEnv* env,
    jobject thiz) {
    
    return g_decoder.getDuration();
}

extern "C" JNIEXPORT void JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeRelease(
    JNIEnv* env,
    jobject thiz) {
    
    g_decoder.release();
}
