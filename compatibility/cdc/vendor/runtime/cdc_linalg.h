#ifndef CDC_LINALG_H
#define CDC_LINALG_H

#include <stddef.h>

/* Small, deterministic row-major linear-algebra surface for U2.  This is
 * deliberately not an eigensolver.  Spectral claims are admitted only
 * through a separately identifiable real-Schur backend and are checked
 * against the input matrix before they are returned. */

typedef enum {
    CDC_LINALG_OK = 0,
    CDC_LINALG_INVALID_ARGUMENT,
    CDC_LINALG_DIMENSION_MISMATCH,
    CDC_LINALG_ALLOCATION_FAILED,
    CDC_LINALG_NONFINITE,
    CDC_LINALG_BACKEND_UNAVAILABLE,
    CDC_LINALG_BACKEND_FAILED,
    CDC_LINALG_VALIDATION_FAILED
} cdc_linalg_status;

typedef struct {
    size_t rows;
    size_t cols;
    double *data;
    int owns_data;
} cdc_matrix;

const char *cdc_linalg_status_reason(cdc_linalg_status status);

cdc_linalg_status cdc_matrix_init(cdc_matrix *matrix, size_t rows, size_t cols);
cdc_linalg_status cdc_matrix_view(cdc_matrix *matrix, size_t rows, size_t cols,
                                  double *storage);
void cdc_matrix_release(cdc_matrix *matrix);
cdc_linalg_status cdc_matrix_zero(cdc_matrix *matrix);
cdc_linalg_status cdc_matrix_identity(cdc_matrix *matrix);
cdc_linalg_status cdc_matrix_copy(cdc_matrix *destination,
                                  const cdc_matrix *source);
cdc_linalg_status cdc_matrix_multiply(cdc_matrix *product,
                                      const cdc_matrix *left,
                                      const cdc_matrix *right);
cdc_linalg_status cdc_matrix_jvp(const cdc_matrix *matrix,
                                 const double *vector, size_t vector_length,
                                 double *product, size_t product_length);
int cdc_matrix_all_finite(const cdc_matrix *matrix);
double cdc_matrix_max_abs(const cdc_matrix *matrix);

/* A backend receives A in row-major form and must fill the real Schur
 * factorization A = Q T Q^T, also row-major.  It does not get to certify
 * itself: cdc_real_schur_compute independently checks reconstruction,
 * orthogonality, quasi-triangular structure, and finiteness. */
typedef cdc_linalg_status (*cdc_real_schur_backend_fn)(
    const cdc_matrix *input,
    cdc_matrix *q,
    cdc_matrix *t,
    double *eigen_real,
    double *eigen_imag,
    void *context);

typedef struct {
    const char *name;
    cdc_real_schur_backend_fn compute;
    void *context;
} cdc_real_schur_backend;

typedef struct {
    size_t dimension;
    cdc_matrix q;
    cdc_matrix t;
    double *eigen_real;
    double *eigen_imag;
    double reconstruction_residual;
    double orthogonality_residual;
    double triangular_residual;
    char backend[32];
} cdc_real_schur_result;

void cdc_real_schur_result_release(cdc_real_schur_result *result);

/* Passing NULL selects the system LAPACK backend.  If LAPACK was not linked,
 * this returns CDC_LINALG_BACKEND_UNAVAILABLE rather than fabricating a
 * spectrum.  validation_tolerance must be finite and positive. */
cdc_linalg_status cdc_real_schur_compute(
    const cdc_matrix *input,
    const cdc_real_schur_backend *backend,
    double validation_tolerance,
    cdc_real_schur_result *result);

/* Returns a backend descriptor only when the weak LAPACK dgees symbol is
 * present in the current process. */
const cdc_real_schur_backend *cdc_system_real_schur_backend(void);

#endif
