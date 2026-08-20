#ifndef _CANVAS_FUNCTIONS_H_
#define _CANVAS_FUNCTIONS_H_

void canvas_fun_1d_histogram (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas);
void canvas_fun_2d_histogram (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas);
void canvas_fun_3d_histogram (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas);
void canvas_fun_3d_line (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas);

#endif /* _CANVAS_FUNCTIONS_H_ */
