#include "matrix_mul.h"

/*
 * 2x2 Matrix-Multiplikation (C = A * B)
 * Alle Matrizen liegen im Row-Major-Format vor (2 Zeilen, 2 Spalten = 4 Floats)
 * C[i][j] = A[i][0]*B[0][j] + A[i][1]*B[1][j]
 */
void matmul_2x2_c(const float *a, const float *b, float *out)
{
    float a00 = a[0], a01 = a[1];
    float a10 = a[2], a11 = a[3];

    float b00 = b[0], b01 = b[1];
    float b10 = b[2], b11 = b[3];

    out[0] = a00 * b00 + a01 * b10;
    out[1] = a00 * b01 + a01 * b11;
    out[2] = a10 * b00 + a11 * b10;
    out[3] = a10 * b01 + a11 * b11;
}

/*
 * 3x3 Matrix-Multiplikation (C = A * B)
 * 3 Zeilen, 3 Spalten = 9 Floats
 * C[i][j] = A[i][0]*B[0][j] + A[i][1]*B[1][j] + A[i][2]*B[2][j]
 */
void matmul_3x3_c(const float *a, const float *b, float *out)
{
    int i, j;
    for (i = 0; i < 3; i++) {
        float a_i0 = a[i * 3 + 0];
        float a_i1 = a[i * 3 + 1];
        float a_i2 = a[i * 3 + 2];
        for (j = 0; j < 3; j++) {
            out[i * 3 + j] = a_i0 * b[0 * 3 + j] +
                             a_i1 * b[1 * 3 + j] +
                             a_i2 * b[2 * 3 + j];
        }
    }
}

/*
 * 4x4 Matrix-Multiplikation (C = A * B)
 * 4 Zeilen, 4 Spalten = 16 Floats (Standard 3D Transformation)
 * C[i][j] = sum_{k=0..3} A[i][k] * B[k][j]
 */
void matmul_4x4_c(const float *a, const float *b, float *out)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        float a_i0 = a[i * 4 + 0];
        float a_i1 = a[i * 4 + 1];
        float a_i2 = a[i * 4 + 2];
        float a_i3 = a[i * 4 + 3];
        for (j = 0; j < 4; j++) {
            out[i * 4 + j] = a_i0 * b[0 * 4 + j] +
                             a_i1 * b[1 * 4 + j] +
                             a_i2 * b[2 * 4 + j] +
                             a_i3 * b[3 * 4 + j];
        }
    }
}
