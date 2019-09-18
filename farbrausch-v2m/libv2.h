#pragma once

/*************************************************************************************/
/*************************************************************************************/
/**                                                                                 **/
/**  LibV2 header file                                                              **/
/**  written Tammo 'kb' Hinrichs 2000-2008                                          **/
/**  This file is in the public domain                                              **/
/**                                                                                 **/
/*************************************************************************************/
/*************************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

  /*************************************************************************************/
  /**                                                                                 **/
  /** DirectSound output code                                                         **/
  /**                                                                                 **/
  /*************************************************************************************/

  // your rendering callback function has to look this way:
  // parm: pointer you specified with dsInit
  // buf: pointer to interleved stereo float destination buffer (1.0=0dB)
  // len: length of buffer in samples (!)
  typedef void (DSIOCALLBACK)(void *parm, float *buf, unsigned long len);

  // initializes DirectSound output.
  // callback: your render callback function
  // parm: a pointer that'll be supplied to the function on every call
  // hWnd: window handle of your application (GetForegroundWindow() works quite well :)
  unsigned long dsInit(DSIOCALLBACK *callback, void *parm, void *hWnd);

  // shuts down DirectSound output
  void dsClose();

  // gets sample-exact and latency compensated current play position
  signed long dsGetCurSmp();

  // sets player volume (default is 1.0)
  void dsSetVolume(float vol);

  // forces rendering thread to update. On single-core CPUs it's a good idea to
  // call this once per frame (improves A/V sync and reduces any stuttering),
  // with more than one CPU it's pretty much useless.
  void dsTick();

  // lock and unlock the sound thread's thread sync lock. If you want to modify
  // any of your sound variables outside the render thread, encapsulate that part
  // of code in between these two functions.
  void dsLock();
  void dsUnlock();

  /*************************************************************************************/
  /**                                                                                 **/
  /** Synthesizer interface                                                           **/
  /**                                                                                 **/
  /*************************************************************************************/

  // returns size of work memory in bytes. Per synthesizer instance reserve at least
  // this amount of memory and supply it as the "pthis" parameter of all other functions
  // Note: If you need only one static instance, 3 Megabytes are a good bet.
  unsigned int synthGetSize();

  // inits synthesizer instance.
  // pthis     : pointer to work mem
  // patchmap  : pointer to patch data
  // samplerate: output sample rate (44100-192000 Hz), use 44100 when playing with dsio
  void synthInit(void *pthis, const void *patchmap, int samplerate=44100);

  // inits global parameters
  // pthis: pointer to work mem
  // ptr  : pointer to global parameters
  void synthSetGlobals(void *pthis, const void *ptr);

  // inits speech synthesizer texts
  // pthis: pointer to work mem
  // ptr  : pointer to text array
  void synthSetLyrics(void *pthis, const char **ptr);

  // renders synth output to destination buffer
  // pthis: pointer to work mem
  // buf  : pointer to interleaved float stereo out buffer
  // smp  : number of samples to render
  // buf2 : if this is specified, the synth will render the left and right channel into
  //        two mono float buffers at buf and buf2 instead of one interleaved buffer
  // add  : if this is specified, the synth will add its output to the destination
  //        buffer instead of replacing its contents
  void synthRender(void *pthis, void *buf, int smp, void *buf2 = 0, int add = 0);

  // pipes a stream of MIDI commands to the synthesizer
  // pthis: pointer to work mem
  // ptr  : pointer to buffer with MIDI data to process.
  //        NOTE: The buffer MUST end with a 0xfd byte
  void synthProcessMIDI(void *pthis, const void *ptr);

  // sets operation mode of VU meters
  // pthis: pointer to work mem
  // mode : 0 for peak meters, 1 for RMS meters
  void synthSetVUMode(void *pthis, int mode); // 0: peak, 1: rms

  // retrieves VU meter data for a channel
  // pthis: pointer to work mem
  // ch   : channel to retrieve (0..15)
  // l    : pointer to float variable where left VU is stored
  // r    : pointer to float variable where right VU is stored
  void synthGetChannelVU(void *pthis, int ch, float *l, float *r); // ch: 0..15

  // retrieves master VU meter
  // pthis: pointer to work mem
  // l    : pointer to float variable where left VU is stored
  // r    : pointer to float variable where right VU is stored
  void synthGetMainVU(void *pthis, float *l, float *r);

#ifdef __cplusplus
}
#endif
