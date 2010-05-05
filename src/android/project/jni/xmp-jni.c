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

	opt->freq = 44100;
	opt->resol = 16;
	opt->outfmt &= ~XMP_FMT_MONO;
	opt->flags |= XMP_CTL_ITPT | XMP_CTL_FILTER;

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

	return 0;
}


JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_load(JNIEnv *env, jobject obj, jstring filename)
{
	if (xmp_load_module(ctx, filename) < 0)
		return -1;
	xmp_get_module_info(ctx, &mi);

	return 0;
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_release(JNIEnv *env, jobject obj)
{
	xmp_release_module(ctx);

	return 0;
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_play(JNIEnv *env, jobject obj)
{
	return xmp_player_start(ctx);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_stop(JNIEnv *env, jobject obj)
{
	xmp_player_end(ctx);

	return 0;
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_playFrame(JNIEnv *env, jobject obj)
{
	return xmp_player_frame(ctx);
}

JNIEXPORT jint JNICALL
Java_org_helllabs_android_Xmp_softmixer(JNIEnv *env, jobject obj)
{
	return xmp_smix_softmixer(ctx);
}

JNIEXPORT jshortArray JNICALL
Java_org_helllabs_android_Xmp_getBuffer(JNIEnv *env, jobject obj, jint size)
{
	jshortArray a;
	short *b;

	if ((a = (*env)->NewShortArray(env, size)) == NULL)
		return NULL;

	b = (short *)xmp_smix_buffer(ctx);
	(*env)->SetShortArrayRegion(env, a, 0, size, b);

	return b;
}
