#ifndef _MACOS_ICON_H_
#define _MACOS_ICON_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void update_macos_icon (const uint32_t *pixels, int width, int height, int bytes_per_row);

#endif /* _MACOS_ICON_H_ */
