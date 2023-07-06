// Copyright(c) 2021 Björn Ottosson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this softwareand associated documentation files(the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions :
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <math.h>
#include "oklab.h"

float srgb_transfer_function (float a)
{
    return .0031308f >= a ? 12.92f * a : 1.055f * powf(a, .4166666666666667f) - .055f;
}

float srgb_transfer_function_inv (float a)
{
    return .04045f < a ? powf((a + .055f) / 1.055f, 2.4f) : a / 12.92f;
}
void srgb_to_linear_srgb (RGB *srgb, RGB *rgb)
{
    rgb->r = srgb_transfer_function_inv (srgb->r);
    rgb->g = srgb_transfer_function_inv (srgb->g);
    rgb->b = srgb_transfer_function_inv (srgb->b);
}

void linear_srgb_to_srgb (RGB *rgb, RGB *srgb)
{
    srgb->r = srgb_transfer_function (rgb->r);
    srgb->g = srgb_transfer_function (rgb->g);
    srgb->b = srgb_transfer_function (rgb->b);
}

void linear_srgb_to_oklab (RGB *rgb, Lab *oklab)
{
    float l = 0.4122214708f * rgb->r + 0.5363325363f * rgb->g + 0.0514459929f * rgb->b;
    float m = 0.2119034982f * rgb->r + 0.6806995451f * rgb->g + 0.1073969566f * rgb->b;
    float s = 0.0883024619f * rgb->r + 0.2817188376f * rgb->g + 0.6299787005f * rgb->b;

    float l_ = cbrtf (l);
    float m_ = cbrtf (m);
    float s_ = cbrtf (s);

    oklab->L = 0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_;
    oklab->a = 1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_;
    oklab->b = 0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_;
}

void oklab_to_linear_srgb (Lab *oklab, RGB *rgb)
{
    float l_ = oklab->L + 0.3963377774f * oklab->a + 0.2158037573f * oklab->b;
    float m_ = oklab->L - 0.1055613458f * oklab->a - 0.0638541728f * oklab->b;
    float s_ = oklab->L - 0.0894841775f * oklab->a - 1.2914855480f * oklab->b;

    float l = l_*l_*l_;
    float m = m_*m_*m_;
    float s = s_*s_*s_;

    rgb->r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    rgb->g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    rgb->b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}
void srgb_to_oklab (RGB *srgb, Lab *oklab)
{
    RGB srgbLinear;
    srgb_to_linear_srgb (srgb, & srgbLinear);
    linear_srgb_to_oklab (& srgbLinear, oklab);
}

void oklab_to_srgb (Lab *oklab, RGB *srgb)
{
    RGB srgbLinear;
    oklab_to_linear_srgb (oklab, & srgbLinear);
    linear_srgb_to_srgb (& srgbLinear, srgb);
}

