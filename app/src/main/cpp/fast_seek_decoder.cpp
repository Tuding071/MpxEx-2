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

extern "C" {

// Simple implementation that returns success but no actual frames
// You can enhance this later with actual media decoding

JNIEXPORT jboolean JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeInit(
    JNIEnv* env,
    jobject thiz,
    jstring videoPath) {
    
    const char* path = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("FastSeekDecoder init with path: %s", path);
    env->ReleaseStringUTFChars(videoPath, path);
    
    // For now, just return true - we'll implement actual decoding later
    return JNI_TRUE;
}

JNIEXPORT jbyteArray JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeSeekTo(
    JNIEnv* env,
    jobject thiz,
    jlong positionMs) {
    
    LOGI("FastSeekDecoder seek to: %ld ms", positionMs);
    
    // Return empty byte array for now
    // In real implementation, you'd decode and return JPEG data
    jbyteArray result = env->NewByteArray(0);
    return result;
}

JNIEXPORT jlong JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeGetDuration(
    JNIEnv* env,
    jobject thiz) {
    
    // Return dummy duration
    return 60000L; // 60 seconds
}

JNIEXPORT void JNICALL
Java_app_marlboroadvance_mpvex_ui_player_FastSeekDecoder_nativeRelease(
    JNIEnv* env,
    jobject thiz) {
    
    LOGI("FastSeekDecoder released");
}

} // extern "C"
