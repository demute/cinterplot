#ifndef _AUDIO_H_
#define _AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_FREQUENCY 48000
void audio_init (void);
float *audio_in_samples_get (void);
void audio_out_samples_push (float *samples);
void audio_close (void);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _AUDIO_H_ */
