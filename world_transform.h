#ifndef _WORLD_TRANSFORM_H_
#define _WORLD_TRANSFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct WorldTransform
{
    // if x is a data point from a CipGraph, data position gets transformed to world
    // coordinates as
    // w = rotMtx * scaleMtx * (x - centerPos),
    // then the xy values are scaled by (1.0 / (z * perspectiveFactor + 1);
    // and the range [-1,1] for x and y is mapped to [0,w-1] and [0,h-1] respectively.
    //
    // If w is a world coordinate, the corresponding data coordinate can be derived using
    // the fact that rotMtxInv = transpose(rotMtx) = t(rotMtx):
    //
    // w = rotMtx * scaleMtx * (x - centerPos)
    // t(rotMtx) * w = scaleMtx * (x - centerPos),
    // scaleMtxInv * t(rotMtx) * w = x - centerPos
    // x = scaleMtxInv * t(rotMtx) * w + centerPos

    // For a translation in world coordinates, define w to be a difference
    // in world coordinates and translate that to data coordinates
    // x = scaleMtxInv * t(rotMtx) * w
    // so a movement w in world coordinates moves the centerPos
    // centerPos += scaleMtxInv * t(rotMtx) * w

    // For a translation in data coordinates, the corresponding coordinate in centerPos
    // is adjusted. The magnitude of the translation must be correct though, so define
    // w = (1,1,1) and translate it to data coordinates and pick the coordinate of interest:
    // dx = scaleMtxInv * t(rotMtx) * w
    // centerPos[i] += step * dx[i]

    // For a rotation in world coordinates around point x,
    // the rotation is applied to the left of rotMtx:
    // w = newRot * rotMtx * scaleMtx * (x - centerPos)
    // so the new rotMtx is replaced by
    // rotMtx = newRot * rotMtx
    //
    // After the rotation, centerPos needs to be adjusted such that the same x-value lands
    // at the same w, so if w0 is the point for x before the rotation and w1 is the point after,
    // we need to make a translation in world coordinates by (w1-w0).
    // adjust centerPos using the same algorithm as for a translation in world coordinates.

    // For a rotation in data coordinates around a point x,
    // we apply the rotation to the right of the transformation:
    // w = rotMtx * scaleMtx * newRot * (x - centerPos)
    // and to absorb this rotation into rotMtx, we define
    // someRot * scaleMtx = scaleMtx * newRot
    // and solve for someRot
    // someRot = scaleMtx * newRot * scaleMtxInv
    // which means rotMtx gets updated by
    // rotMtx = rotMtx * scaleMtx * newRot * scaleMtxInv
    //
    // Like rotation in world coordinates, we compute (w0-w1) and translate for the movement of x
    // caused by the rotation, which means we translate the world by (w1-w0)

    // For a scaling in world coordinates around a point x,
    // define a scaling matrix S=I; S[i][i] = f, where i = 0,1 or 2 and f is the scale factor,
    // apply it to the left of everything to get
    // w = newScale * rotMtx * scaleMtx * (x - centerPos)
    // and to absorb it into scaleMtx, define
    // newScale * rotMtx = rotMtx * someScale
    // and solve for someScale
    // someScale = rotMtxInv * newScale * rotMtx
    // update scaleMtx with these new factors
    // scaleMtx = rotMtxInv * newScale * rotMtx * scaleMtx
    //
    // We need to update scaleMtxInv as well, so given that we knew scaleMtxInv before, we can use it to update it:
    // newScale^-1 is trivial as newScale is diagonal
    // someScaleInv = rotMtxInv * newScale * rotMtx
    // So the inverse of someScale * scaleMtx becomes
    // scaleMtxInv * someScaleInv, so update scaleMtxInv with
    // scaleMtxInv = scaleMtxInv * someScaleInv;
    //
    //
    // Like for the other transformations, track w0 and w1 as before and compensate for the shift.

    // To compute the 2d projection from world coordinates, apply the perspectiveFactor
    // s[0] = w[0] / (w[2] * perspectiveFactor + 1)
    // s[1] = w[1] / (w[2] * perspectiveFactor + 1)

    double rotMtx[3][3];
    double rotMtxInv[3][3];
    double scaleMtx[3][3];
    double scaleMtxInv[3][3];
    double centerPos[3];
    double perspectiveFactor;
} WorldTransform;


void world_transform_adjust_centerpos_using_scaled_diff (WorldTransform *world, double sd[3]);
void world_transform_adjust_centerpos_using_world_diff (WorldTransform *world, double wd[3]);
int  world_transform_datapos_to_bin (WorldTransform *world, double x[3], int w, int h, int *xi, int *yi, double *wz);
void world_transform_bin_to_datapos (WorldTransform *world, int w, int h, int xi, int yi, double wz, double x[3]);
int  world_transform_datapos_to_projected (WorldTransform *world, double x[3], double p[3]);
void world_transform_datapos_to_worldpos (WorldTransform *world, double x[3], double w[3]);
void world_transform_projected_to_datapos (WorldTransform *world, double p[3], double d[3]);
void world_transform_projected_to_worldpos (WorldTransform *world, double p[3], double w[3]);
void world_transform_rotate_data (WorldTransform *world, double datapos[3], int axis, double theta);
void world_transform_rotate_world (WorldTransform *world, double datapos[3], int axis, double theta);
void world_transform_scale_world (WorldTransform *world, double datapos[3], double scale[3]);
void world_transform_scale_data (WorldTransform *world, double datapos[3], double scale[3]);
void world_transform_set_default_values (WorldTransform *world);
void world_transform_set_range (WorldTransform *world, int axis, double range[2], double margin);
void world_transform_set_ranges (WorldTransform *world, double ranges[3][2], double margin);
void world_transform_worldpos_to_datapos (WorldTransform *world, double w[3], double x[3]);
int  world_transform_worldpos_to_projected (WorldTransform *world, double w[3], double p[3]);
void world_dump (WorldTransform *world, int line);
void world_transform_adjust_worldz (WorldTransform *world, double datapos[3], double wz, double newPerspectiveFactor);
void world_transform_zero_wz (WorldTransform *world, double datapos[3]);
void world_apply_constraints (WorldTransform *world);

#ifdef __cplusplus
} /* end extern C */
#endif

#define WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT(world)  \
    double mtx[3][3];                                        \
    double (*mtx1)[3] = world->rotMtx;                       \
    double (*mtx2)[3] = world->scaleMtx;                     \
    for (int i=0; i<3; i++)                                  \
        for (int j=0; j<3; j++)                              \
            mtx[i][j] =                                      \
                mtx1[i][0] * mtx2[0][j] +                    \
                mtx1[i][1] * mtx2[1][j] +                    \
                mtx1[i][2] * mtx2[2][j];                     \
    const double sx = 0.5 * (w - 1);                         \
    const double sy = 0.5 * (h - 1);                         \
    const double cx0 = world->centerPos[0];                  \
    const double cx1 = world->centerPos[1];                  \
    const double cx2 = world->centerPos[2];                  \
    const double m00 = mtx[0][0];                            \
    const double m01 = mtx[0][1];                            \
    const double m02 = mtx[0][2];                            \
    const double m10 = mtx[1][0];                            \
    const double m11 = mtx[1][1];                            \
    const double m12 = mtx[1][2];                            \
    const double m20 = mtx[2][0];                            \
    const double m21 = mtx[2][1];                            \
    const double m22 = mtx[2][2];                            \
    const double pf  = world->perspectiveFactor;

// last check xf >= 0.0 is used to make casting of NaNs to -1.
// the mask is used to or the result with -1 = 0xfff... if zz is
// less than the required amount to give an unaliased projection
#define WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE(datapos,xi,yi,wz) \
        const double lx = datapos[0] - cx0;         \
        const double ly = datapos[1] - cx1;         \
        const double lz = datapos[2] - cx2;         \
        const double wx = m00*lx + m01*ly + m02*lz; \
        const double wy = m10*lx + m11*ly + m12*lz; \
        const double wz = m20*lx + m21*ly + m22*lz; \
        const double zz = wz*pf + 1.0;              \
        const int    mask = (zz > 1e-3) - 1;        \
        const double iz = 1.0 / zz;                 \
        const double xf = (wx * iz + 1.0) * sx;     \
        const double yf = (wy * iz + 1.0) * sy;     \
        const int xi = (xf >= 0.0) ? (int) xf | mask : -1; \
        const int yi = (yf >= 0.0) ? (int) yf        : -1; \

#endif /* _WORLD_TRANSFORM_H_ */
