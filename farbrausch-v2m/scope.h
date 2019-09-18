#ifndef __SCOPE_H__INCLUDED__
#define __SCOPE_H__INCLUDED__

extern void scopeOpen(const void *unique_id, const char *title, int nsamples, int w, int h);
extern void scopeClose(const void *unique_id);
extern bool scopeIsOpen(const void *unique_id);
extern void scopeSubmit(const void *unique_id, const float *data, int nsamples);
extern void scopeSubmitStrided(const void *unique_id, const float *data, int stride, int nsamples);
extern void scopeUpdateAll();

#ifdef EMSCRIPTEN
extern "C" int scope_activated;
extern float** get_scope_buffers();
extern const char** get_scope_titles();
#endif

#endif