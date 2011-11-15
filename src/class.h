#ifndef __CLASS_H__
#define __CLASS_H__

/* class initializations */
void* _mdxmml_ym2151_initialize(void);
void  _mdxmml_ym2151_finalize(void* self);

void* _pcm8_initialize(void);
void  _pcm8_finalize(void* in_self);

void* _mdx2151_initialize(void);
void  _mdx2151_finalize(void* in_self);

void* _ym2151_c_initialize(void);
void  _ym2151_c_finalize(void* in_self);

void* ym2151_instance(void);

/* interfaces */
void* _get_mdx2151(void);
void* _get_mdxmml_ym2151(void);
void* _get_pcm8(void);
void* _get_ym2151_c(void);


void pcm8_clear_buffer_flush_flag(void);
int pcm8_buffer_flush_flag(void);



#endif