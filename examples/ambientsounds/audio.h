#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_FREQUENCY 48000
void audio_out_enqueue (float sample);
void audio_init (void);
float audio_in_sample_get (void);
void audio_out_sample_push (float sample);
void audio_close (void);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _AUDIO_H_ */
