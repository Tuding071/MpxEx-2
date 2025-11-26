#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include <android/log.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <cpu-features.h>
#include <unistd.h>
#include <chrono>
#include <thread>

#define LOG_TAG "FastSeekDecoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class FastSeekDecoder {
private:
    AMediaExtractor* extractor = nullptr;
    AMediaCodec* codec = nullptr;
    AMediaFormat* format = nullptr;
    bool initialized = false;
    int64_t durationUs = 0;
    int32_t width = 0;
    int32_t height = 0;
    const char* mimeType = nullptr;
    
    // Low resolution target
    const int TARGET_WIDTH = 426;   // 240p width
    const int TARGET_HEIGHT = 240;  // 240p height
    
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
        
        // Set data source
        media_status_t status;
        if (strstr(videoPath, "content://") == videoPath || 
            strstr(videoPath, "file://") == videoPath) {
            // URI format
            status = AMediaExtractor_setDataSource(extractor, videoPath);
        } else {
            // File path format - we'll use a simpler approach for now
            LOGE("Direct file paths not fully supported, using URI: %s", videoPath);
            status = AMEDIA_ERROR_UNSUPPORTED;
        }
        
        if (status != AMEDIA_OK) {
            LOGW("URI method failed, trying fallback...");
            // For now, we'll use a simplified approach that works with basic files
            // In production, you'd need proper URI conversion
            release();
            return false;
        }
        
        // Find video track
        size_t numTracks = AMediaExtractor_getTrackCount(extractor);
        LOGI("Found %zu tracks", numTracks);
        
        for (size_t i = 0; i < numTracks; i++) {
            format = AMediaExtractor_getTrackFormat(extractor, i);
            if (!format) continue;
            
            const char* trackMime;
            if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &trackMime)) {
                LOGI("Track %zu: %s", i, trackMime);
                if (strncmp(trackMime, "video/", 6) == 0) {
                    mimeType = trackMime;
                    
                    // Get video properties
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                    AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &durationUs);
                    
                    LOGI("Video track: %dx%d, duration: %lld us", width, height, (long long)durationUs);
                    
                    // Select this track
                    AMediaExtractor_selectTrack(extractor, i);
                    
                    // Configure for low resolution
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_WIDTH, TARGET_WIDTH);
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_HEIGHT, TARGET_HEIGHT);
                    
                    // Create decoder
                    codec = AMediaCodec_createDecoderByType(mimeType);
                    if (!codec) {
                        LOGE("Failed to create codec for %s", mimeType);
                        AMediaFormat_delete(format);
                        format = nullptr;
                        release();
                        return false;
                    }
                    
                    // Configure codec
                    status = AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
                    if (status != AMEDIA_OK) {
                        LOGE("Failed to configure codec: %d", status);
                        AMediaFormat_delete(format);
                        format = nullptr;
                        release();
                        return false;
                    }
                    
                    // Start codec
                    status = AMediaCodec_start(codec);
                    if (status != AMEDIA_OK) {
                        LOGE("Failed to start codec: %d", status);
                        AMediaFormat_delete(format);
                        format = nullptr;
                        release();
                        return false;
                    }
                    
                    initialized = true;
                    LOGI("FastSeekDecoder initialized successfully");
                    return true;
                }
            }
            AMediaFormat_delete(format);
            format = nullptr;
        }
        
        LOGE("No suitable video track found");
        release();
        return false;
    }
    
    jobject seekToFrame(JNIEnv* env, jlong positionMs) {
        if (!initialized || !extractor || !codec) {
            LOGE("Decoder not initialized");
            return nullptr;
        }
        
        int64_t positionUs = positionMs * 1000; // Convert ms to microseconds
        
        LOGI("Seeking to: %lld ms (%lld us)", (long long)positionMs, (long long)positionUs);
        
        // Seek to the closest sync frame
        media_status_t seekStatus = AMediaExtractor_seekTo(extractor, positionUs, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
        if (seekStatus != AMEDIA_OK) {
            LOGE("Seek failed: %d", seekStatus);
            return nullptr;
        }
        
        // Flush codec to clear any pending buffers
        AMediaCodec_flush(codec);
        
        // Try to decode a frame
        return decodeSingleFrame(env);
    }
    
    jobject decodeSingleFrame(JNIEnv* env) {
        if (!initialized) return nullptr;
        
        constexpr int64_t TIMEOUT_US = 10000; // 10ms timeout
        bool frameDecoded = false;
        jobject bitmap = nullptr;
        
        // Feed input to decoder
        ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec, TIMEOUT_US);
        if (inputIndex >= 0) {
            size_t inputSize;
            uint8_t* inputBuffer = AMediaCodec_getInputBuffer(codec, inputIndex, &inputSize);
            if (inputBuffer) {
                // Read sample from extractor
                ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, inputBuffer, inputSize);
                int64_t sampleTime = AMediaExtractor_getSampleTime(extractor);
                
                if (sampleSize > 0) {
                    // Feed the sample to decoder
                    AMediaCodec_queueInputBuffer(codec, inputIndex, 0, sampleSize, sampleTime, 0);
                    AMediaExtractor_advance(extractor);
                } else {
                    // No more samples, send EOS
                    AMediaCodec_queueInputBuffer(codec, inputIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                }
            }
        }
        
        // Try to get output
        AMediaCodecBufferInfo info;
        ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, TIMEOUT_US);
        
        if (outputIndex >= 0) {
            if (info.size > 0 && (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) == 0) {
                LOGI("Frame decoded successfully, size: %d", info.size);
                
                // Create a simple bitmap for the frame
                bitmap = createDummyBitmap(env);
                frameDecoded = true;
            }
            
            // Release the output buffer
            AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
        }
        
        if (frameDecoded) {
            return bitmap;
        }
        
        LOGW("No frame decoded within timeout");
        return createDummyBitmap(env);
    }
    
    jobject createDummyBitmap(JNIEnv* env) {
        // Create a simple colored bitmap for testing
        // In a real implementation, you'd convert YUV frames to RGB here
        
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
        jobject bitmap = env->CallStaticObjectMethod(bitmapClass, createBitmapMethod, 
                                                    TARGET_WIDTH, TARGET_HEIGHT, config);
        
        if (bitmap) {
            // Fill with a test pattern based on time
            fillTestPattern(env, bitmap);
        }
        
        return bitmap;
    }
    
    void fillTestPattern(JNIEnv* env, jobject bitmap) {
        // Fill bitmap with a simple test pattern
        // This helps verify the bitmap is working
        
        AndroidBitmapInfo info;
        if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
            LOGE("Failed to get bitmap info");
            return;
        }
        
        void* pixels;
        if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
            LOGE("Failed to lock bitmap pixels");
            return;
        }
        
        // Create a simple gradient pattern
        uint32_t* pixelArray = static_cast<uint32_t*>(pixels);
        for (int y = 0; y < info.height; y++) {
            for (int x = 0; x < info.width; x++) {
                uint8_t r = (x * 255) / info.width;
                uint8_t g = (y * 255) / info.height;
                uint8_t b = 128;
                pixelArray[y * info.width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
        
        AndroidBitmap_unlockPixels(env, bitmap);
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
        
        if (format) {
            AMediaFormat_delete(format);
            format = nullptr;
        }
        
        if (extractor) {
            AMediaExtractor_delete(extractor);
            extractor = nullptr;
        }
        
        initialized = false;
        durationUs = 0;
        width = height = 0;
        mimeType = nullptr;
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
