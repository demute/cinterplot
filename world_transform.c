#include "cinterplot_common.h"
#include "world_transform.h"

static void matrix_matrix_multiply (double dst[3][3], double mtx1[3][3], double mtx2[3][3])
{
    double tmp[3][3];
    for (int i=0; i<3; i++)
    {
        for (int j=0; j<3; j++)
        {
            tmp[i][j] =
                mtx1[i][0] * mtx2[0][j] +
                mtx1[i][1] * mtx2[1][j] +
                mtx1[i][2] * mtx2[2][j];
        }
    }
    memcpy (dst, tmp, sizeof (tmp));
}

static void matrix_chain_multiply(double dst[3][3], double first[3][3], ...)
{
    double tmp[3][3];
    va_list args;

    memcpy (tmp, first, sizeof (tmp));

    va_start (args, first);

    for (;;)
    {
        double (*next)[3] = va_arg (args, double (*)[3]);

        if (next == NULL)
            break;

        matrix_matrix_multiply (tmp, tmp, next);
    }

    va_end (args);

    memcpy (dst, tmp, sizeof tmp);
}

static void matrix_vector_multiply (double mtx[3][3], double src[3], double dst[3])
{
    double tmp[3];
    tmp[0] = mtx[0][0] * src[0] + mtx[0][1] * src[1] + mtx[0][2] * src[2];
    tmp[1] = mtx[1][0] * src[0] + mtx[1][1] * src[1] + mtx[1][2] * src[2];
    tmp[2] = mtx[2][0] * src[0] + mtx[2][1] * src[1] + mtx[2][2] * src[2];
    dst[0] = tmp[0];
    dst[1] = tmp[1];
    dst[2] = tmp[2];
}

static void matrix_transpose_vector_multiply (double mtx[3][3], double src[3], double dst[3])
{
    double tmp[3];
    tmp[0] = mtx[0][0] * src[0] + mtx[1][0] * src[1] + mtx[2][0] * src[2];
    tmp[1] = mtx[0][1] * src[0] + mtx[1][1] * src[1] + mtx[2][1] * src[2];
    tmp[2] = mtx[0][2] * src[0] + mtx[1][2] * src[1] + mtx[2][2] * src[2];
    dst[0] = tmp[0];
    dst[1] = tmp[1];
    dst[2] = tmp[2];
}

// unused
//static inline void vector_add (double dst[3], double v1[3], double v2[3])
//{
//    dst[0] = v1[0] + v1[0];
//    dst[1] = v1[1] + v1[1];
//    dst[2] = v1[2] + v1[2];
//}

static inline void vector_subtract (double dst[3], double v1[3], double v2[3])
{
    dst[0] = v1[0] - v1[0];
    dst[1] = v1[1] - v1[1];
    dst[2] = v1[2] - v1[2];
}


static inline void make_diagonal_matrix (double mtx[3][3], double vec[3])
{
    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++)
            mtx[i][j] = (i==j) ? vec[i] : 0;
}

static void make_rotation_matrix (double r[3][3], int axis, double theta)
{
    if (axis < 0 || axis > 2)
        exit_error ("invalid axis");

    double c = cos(theta);
    double s = sin(theta);

    double t[3][3][3] =
    {
        {
            { 1,  0,  0},
            { 0,  c, -s},
            { 0,  s,  c},
        },
        {
            { c,  0,  s},
            { 0,  1,  0},
            {-s,  0,  c},
        },
        {
            { c, -s,  0},
            { s,  c,  0},
            { 0,  0,  1},
        }
    };

    memcpy (r, t[axis], sizeof (t[axis]));
}

static inline void make_transpose_matrix (double dst[3][3], double src[3][3])
{
    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++)
            dst[i][j] = src[j][i];
}

// unused
//static void normalise_matrix (double mtx[3][3])
//{
//    double norm;
//
//    // Normalize first column
//    norm = sqrt(mtx[0][0] * mtx[0][0] + mtx[1][0] * mtx[1][0] + mtx[2][0] * mtx[2][0]);
//    mtx[0][0] /= norm;
//    mtx[1][0] /= norm;
//    mtx[2][0] /= norm;
//
//    // Make second column orthogonal to first and normalize
//    double dot = mtx[0][0] * mtx[0][1] + mtx[1][0] * mtx[1][1] + mtx[2][0] * mtx[2][1];
//    mtx[0][1] -= dot * mtx[0][0];
//    mtx[1][1] -= dot * mtx[1][0];
//    mtx[2][1] -= dot * mtx[2][0];
//
//    norm = sqrt(mtx[0][1] * mtx[0][1] + mtx[1][1] * mtx[1][1] + mtx[2][1] * mtx[2][1]);
//    mtx[0][1] /= norm;
//    mtx[1][1] /= norm;
//    mtx[2][1] /= norm;
//
//    // Compute third column as cross product of first two
//    mtx[0][2] = mtx[1][0] * mtx[2][1] - mtx[2][0] * mtx[1][1];
//    mtx[1][2] = mtx[2][0] * mtx[0][1] - mtx[0][0] * mtx[2][1];
//    mtx[2][2] = mtx[0][0] * mtx[1][1] - mtx[1][0] * mtx[0][1];
//}

void world_transform_datapos_to_worldpos (WorldTransform *world, double x[3], double w[3])
{
    double *cx = world->centerPos;
    double localx[3], scaledx[3];

    localx[0] = x[0] - cx[0];
    localx[1] = x[1] - cx[1];
    localx[2] = x[2] - cx[2];

    matrix_vector_multiply (world->scaleMtx, localx, scaledx);
    matrix_vector_multiply (world->rotMtx,   scaledx, w);
}

void world_transform_worldpos_to_datapos (WorldTransform *world, double w[3], double x[3])
{
    double scaledx[3], localx[3];

    matrix_transpose_vector_multiply (world->rotMtx, w, scaledx);
    matrix_vector_multiply (world->scaleMtxInv, scaledx, localx);

    x[0] = localx[0] + world->centerPos[0];
    x[1] = localx[0] + world->centerPos[1];
    x[2] = localx[0] + world->centerPos[2];
}

void world_transform_worldpos_to_projected (WorldTransform *world, double w[3], double p[3])
{
    p[0] = w[0] / (w[2] * world->perspectiveFactor + 1);
    p[1] = w[1] / (w[2] * world->perspectiveFactor + 1);
    p[2] = w[2];
}

void world_transform_projected_to_worldpos (WorldTransform *world, double p[3], double w[3])
{
    w[0] = p[0] * (p[2] * world->perspectiveFactor + 1);
    w[1] = p[1] * (p[2] * world->perspectiveFactor + 1);
    w[2] = p[2];
}

void world_transform_projected_to_datapos (WorldTransform *world, double p[3], double d[3])
{
    double w[3];
    world_transform_projected_to_worldpos (world, p, w);
    world_transform_worldpos_to_datapos (world, w, d);
}

void world_transform_datapos_to_projected (WorldTransform *world, double x[3], double p[3])
{
    double w;

    world_transform_datapos_to_worldpos (world, x, & w);
    world_transform_worldpos_to_projected (world, & w, p);
}

int  world_transform_datapos_to_bin (WorldTransform *world, double x[3], int w, int h, int *xi, int *yi, double *pz)
{
    double p[3];
    world_transform_datapos_to_projected (world, x, p);
    if (isinf (p[0]) || isinf (p[1]) || isnan (p[0]) || isnan (p[1]))
        return -1;

    *xi = (0.5 * p[0] + 0.5) * (w-1);
    *yi = (0.5 * p[1] + 0.5) * (h-1);
    if (pz)
        *pz = p[2];

    return (*xi >= 0 && *yi >= 0 && *xi < w && *yi < h) ? 0 : 1;
}

void world_transform_bin_to_datapos (WorldTransform *world, int w, int h, int xi, int yi, double x[3])
{
    double wpos[3] =
    {
        2 * xi * (1.0 / (w-1)) - 1,
        2 * yi * (1.0 / (h-1)) - 1,
        0
    };

    world_transform_worldpos_to_datapos (world, wpos, x);
}

void world_transform_adjust_centerpos_using_world_diff (WorldTransform *world, double wd[3])
{
    double scaledx[3], localx[3];

    matrix_transpose_vector_multiply (world->rotMtx, wd, scaledx);
    matrix_vector_multiply (world->scaleMtxInv, scaledx, localx);

    world->centerPos[0] += localx[0];
    world->centerPos[1] += localx[1];
    world->centerPos[2] += localx[2];
}

void world_transform_adjust_centerpos_using_scaled_diff (WorldTransform *world, double sd[3])
{
    double wd[3] = {1,1,1};
    double scaledx[3], localx[3];

    matrix_transpose_vector_multiply (world->rotMtx, wd, scaledx);
    matrix_vector_multiply (world->scaleMtxInv, scaledx, localx);

    world->centerPos[0] += localx[0] * sd[0];
    world->centerPos[1] += localx[1] * sd[1];
    world->centerPos[2] += localx[2] * sd[2];
}

void world_transform_rotate_world (WorldTransform *world, double datapos[3], int axis, double theta)
{
    double w0[3], w1[3];
    world_transform_datapos_to_worldpos (world, datapos, w0);

    double newRotMtx[3][3];
    make_rotation_matrix (newRotMtx, axis, theta);
    matrix_matrix_multiply (world->rotMtx, newRotMtx, world->rotMtx);
    make_transpose_matrix (world->rotMtxInv, world->rotMtx);

    world_transform_datapos_to_worldpos (world, datapos, w1);
    vector_subtract (w0, w0, w1);
    world_transform_adjust_centerpos_using_world_diff (world, w0);
}

void world_transform_rotate_data (WorldTransform *world, double datapos[3], int axis, double theta)
{
    double w0[3], w1[3];
    world_transform_datapos_to_worldpos (world, datapos, w0);

    double newRotMtx[3][3];
    make_rotation_matrix (newRotMtx, axis, theta);
    matrix_chain_multiply (world->rotMtx,
                           world->rotMtx, world->scaleMtx, newRotMtx, world->scaleMtxInv, NULL);

    make_transpose_matrix (world->rotMtxInv, world->rotMtx);

    world_transform_datapos_to_worldpos (world, datapos, w1);
    vector_subtract (w0, w0, w1);
    world_transform_adjust_centerpos_using_world_diff (world, w0);
}

void world_transform_scale_world (WorldTransform *world, double datapos[3], double scale[3])
{
    double w0[3], w1[3];
    world_transform_datapos_to_worldpos (world, datapos, w0);

    double scaleInv[3] = {1.0/scale[0], 1.0/scale[1], 1.0/scale[2]};

    double worldScaleFactors[3][3], worldScaleFactorsInv[3][3];

    make_diagonal_matrix (worldScaleFactors,    scale);
    make_diagonal_matrix (worldScaleFactorsInv, scaleInv);

    matrix_chain_multiply (world->scaleMtx,
                           world->rotMtxInv, worldScaleFactors, world->rotMtx, world->scaleMtx, NULL);

    matrix_chain_multiply (world->scaleMtxInv,
                           world->scaleMtxInv, world->rotMtxInv, worldScaleFactorsInv, world->rotMtx, NULL);

    world_transform_datapos_to_worldpos (world, datapos, w1);
    vector_subtract (w0, w0, w1);
    world_transform_adjust_centerpos_using_world_diff (world, w0);
}

void world_transform_scale_orthogonal (WorldTransform *world, double scaling[3])
{
    make_diagonal_matrix (world->scaleMtx, scaling);
}

void world_transform_set_range (WorldTransform *world, int axis, double range[2], double margin)
{
    // FIXME: This function is zeroing off-diagonal elements.
    //        Either make it correct or explain why this is correct.

    for (int i=0; i<3; i++)
        for (int j=0; j<3; j++)
            if (i!=j)
            {
                world->scaleMtx[i][j] = 0;
                world->scaleMtxInv[i][j] = 0;
            }

    double min = range[0];
    double max = range[1];

    double center = 0.5 *  max + 0.5 * min;
    double scale  = (0.5 * max - 0.5 * min) * (1 + margin);

    world->centerPos[axis] = center;
    world->scaleMtx[axis][axis] = scale;

    for (int i=0; i<3; i++)
        world->scaleMtxInv[i][i] = 1.0 / world->scaleMtx[i][i];
}

void world_transform_set_ranges (WorldTransform *world, double ranges[3][2], double margin)
{
    world_transform_set_range (world, 0, ranges[0], margin);
    world_transform_set_range (world, 1, ranges[1], margin);
    world_transform_set_range (world, 2, ranges[2], margin);
}

void world_transform_set_default_values (WorldTransform *world)
{
    bzero (world, sizeof (*world));

    for (int i=0; i<3; i++)
    {
        world->rotMtx[i][i] = 1.0;
        world->rotMtxInv[i][i] = 1.0;
        world->scaleMtx[i][i] = 1.0;
        world->scaleMtxInv[i][i] = 1.0;
        world->centerPos[i] = 0.0;
    }
    world->perspectiveFactor = 0;
}
