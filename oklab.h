#ifndef _OKLAB_H_
#define _OKLAB_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float L;
    float a;
    float b;
} Lab;

typedef struct
{
    float r;
    float g;
    float b;
} RGB;

typedef struct
{
    float h;
    float s;
    float v;
} HSV;

float srgb_transfer_function(float a);
float srgb_transfer_function_inv(float a);
void srgb_to_linear_srgb (RGB *srgb, RGB *rgb);
void linear_srgb_to_srgb (RGB *rgb, RGB *srgb);
void linear_srgb_to_oklab (RGB *rgb, Lab *oklab);
void oklab_to_linear_srgb (Lab *oklab, RGB *rgb);
void srgb_to_oklab (RGB *srgb, Lab *oklab);
void oklab_to_srgb (Lab *oklab, RGB *srgb);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _OKLAB_H_ */
