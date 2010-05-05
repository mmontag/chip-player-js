/* Simple interface adaptor for jni */

#include <stdlib.h>
#include <jni.h>
#include "xmp.h"

extern struct xmp_drv_info drv_smix;

static xmp_context ctx;
static struct xmp_options *opt;
static struct xmp_module_info mi;

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_init(JNIEnv *env, jobject obj)
{
	xmp_drv_register(&drv_smix);
	ctx = xmp_create_context();
	xmp_init(ctx, 0, NULL);
	opt = xmp_get_options(ctx);
	opt->verbosity = 0;

	if (xmp_open_audio(ctx) < 0) {
		xmp_deinit(ctx);
		xmp_free_context(ctx);
		return -1;
	}

	return 0;
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_deinit(JNIEnv *env, jobject obj)
{
	xmp_close_audio(ctx);
	xmp_deinit(ctx);
	xmp_free_context(ctx);
}


JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_load(JNIEnv *env, jobject obj, jstring filename)
{
	if (xmp_load_module(ctx, filename) < 0)
		return -1;
	xmp_get_module_info(ctx, &mi);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_release(JNIEnv *env, jobject obj)
{
	xmp_release_module(ctx);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_play(JNIEnv *env, jobject obj)
{
	xmp_player_start(ctx);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_stop(JNIEnv *env, jobject obj)
{
	xmp_player_end(ctx);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_frame(JNIEnv *env, jobject obj)
{
	xmp_player_frame(ctx);
}

void j_get_buffer(void **data, int *size)
{
	xmp_get_buffer(ctx, data, size);
}

