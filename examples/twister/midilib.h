#ifndef _MIDILIB_H_
#define _MIDILIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

void *midi_init (const char *name);
void  midi_connect (void *_dev);
int   midi_get_message (void *_dev, uint8_t *msg, int *len);
void  midi_cleanup (void *_dev);
int   midi_send_message (void *_dev, uint8_t *buf, int len);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _MIDILIB_H_ */
