#include "cdc_linalg.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CDC_U2_LAPACK_DGEES
typedef int (*cdc_dgees_select_fn)(double *, double *);

/* This declaration is compiled only for the explicit spectral build.  The
 * ordinary runtime stays linkable without LAPACK and holds spectral work with
 * an honest backend-unavailable reason. */
extern void dgees_(char *jobvs, char *sort, cdc_dgees_select_fn select,
                   int *n, double *a, int *lda, int *sdim, double *wr,
                   double *wi, double *vs, int *ldvs, double *work,
                   int *lwork, int *bwork, int *info);
#endif

static int matrix_shape_valid(const cdc_matrix *matrix) {
    return matrix && matrix->rows > 0 && matrix->cols > 0 && matrix->data;
}

const char *cdc_linalg_status_reason(cdc_linalg_status status) {
    switch (status) {
    case CDC_LINALG_OK:
        return "none";
    case CDC_LINALG_INVALID_ARGUMENT:
        return "invalid-argument";
    case CDC_LINALG_DIMENSION_MISMATCH:
        return "dimension-mismatch";
    case CDC_LINALG_ALLOCATION_FAILED:
        return "allocation-failed";
    case CDC_LINALG_NONFINITE:
        return "nonfinite-linear-algebra";
    case CDC_LINALG_BACKEND_UNAVAILABLE:
        return "spectral-backend-unavailable";
    case CDC_LINALG_BACKEND_FAILED:
        return "spectral-backend-failed";
    case CDC_LINALG_VALIDATION_FAILED:
        return "spectral-validation-failed";
    }
    return "unknown-linear-algebra-failure";
}

cdc_linalg_status cdc_matrix_init(cdc_matrix *matrix, size_t rows, size_t cols) {
    if (!matrix || rows == 0 || cols == 0 || rows > SIZE_MAX / cols ||
        rows * cols > SIZE_MAX / sizeof(double)) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    memset(matrix, 0, sizeof(*matrix));
    matrix->data = (double *)calloc(rows * cols, sizeof(double));
    if (!matrix->data) {
        return CDC_LINALG_ALLOCATION_FAILED;
    }
    matrix->rows = rows;
    matrix->cols = cols;
    matrix->owns_data = 1;
    return CDC_LINALG_OK;
}

cdc_linalg_status cdc_matrix_view(cdc_matrix *matrix, size_t rows, size_t cols,
                                  double *storage) {
    if (!matrix || !storage || rows == 0 || cols == 0 || rows > SIZE_MAX / cols) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    matrix->rows = rows;
    matrix->cols = cols;
    matrix->data = storage;
    matrix->owns_data = 0;
    return CDC_LINALG_OK;
}

void cdc_matrix_release(cdc_matrix *matrix) {
    if (!matrix) {
        return;
    }
    if (matrix->owns_data) {
        free(matrix->data);
    }
    memset(matrix, 0, sizeof(*matrix));
}

cdc_linalg_status cdc_matrix_zero(cdc_matrix *matrix) {
    if (!matrix_shape_valid(matrix)) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    memset(matrix->data, 0, matrix->rows * matrix->cols * sizeof(double));
    return CDC_LINALG_OK;
}

cdc_linalg_status cdc_matrix_identity(cdc_matrix *matrix) {
    size_t i;
    cdc_linalg_status status;
    if (!matrix_shape_valid(matrix) || matrix->rows != matrix->cols) {
        return CDC_LINALG_DIMENSION_MISMATCH;
    }
    status = cdc_matrix_zero(matrix);
    if (status != CDC_LINALG_OK) {
        return status;
    }
    for (i = 0; i < matrix->rows; ++i) {
        matrix->data[i * matrix->cols + i] = 1.0;
    }
    return CDC_LINALG_OK;
}

cdc_linalg_status cdc_matrix_copy(cdc_matrix *destination,
                                  const cdc_matrix *source) {
    if (!matrix_shape_valid(destination) || !matrix_shape_valid(source)) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    if (destination->rows != source->rows || destination->cols != source->cols) {
        return CDC_LINALG_DIMENSION_MISMATCH;
    }
    memmove(destination->data, source->data,
            source->rows * source->cols * sizeof(double));
    return CDC_LINALG_OK;
}

cdc_linalg_status cdc_matrix_multiply(cdc_matrix *product,
                                      const cdc_matrix *left,
                                      const cdc_matrix *right) {
    size_t i;
    size_t j;
    size_t k;
    size_t count;
    double *destination;
    double *temporary = NULL;
    if (!matrix_shape_valid(product) || !matrix_shape_valid(left) ||
        !matrix_shape_valid(right)) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    if (left->cols != right->rows || product->rows != left->rows ||
        product->cols != right->cols) {
        return CDC_LINALG_DIMENSION_MISMATCH;
    }
    count = product->rows * product->cols;
    destination = product->data;
    if (product->data == left->data || product->data == right->data) {
        temporary = (double *)calloc(count, sizeof(double));
        if (!temporary) {
            return CDC_LINALG_ALLOCATION_FAILED;
        }
        destination = temporary;
    } else {
        memset(destination, 0, count * sizeof(double));
    }
    for (i = 0; i < left->rows; ++i) {
        for (k = 0; k < left->cols; ++k) {
            double scale = left->data[i * left->cols + k];
            for (j = 0; j < right->cols; ++j) {
                destination[i * right->cols + j] +=
                    scale * right->data[k * right->cols + j];
            }
        }
    }
    if (temporary) {
        memcpy(product->data, temporary, count * sizeof(double));
        free(temporary);
    }
    return cdc_matrix_all_finite(product) ? CDC_LINALG_OK : CDC_LINALG_NONFINITE;
}

cdc_linalg_status cdc_matrix_jvp(const cdc_matrix *matrix,
                                 const double *vector, size_t vector_length,
                                 double *product, size_t product_length) {
    size_t i;
    size_t j;
    if (!matrix_shape_valid(matrix) || !vector || !product) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    if (vector_length != matrix->cols || product_length != matrix->rows) {
        return CDC_LINALG_DIMENSION_MISMATCH;
    }
    for (i = 0; i < matrix->rows; ++i) {
        double total = 0.0;
        for (j = 0; j < matrix->cols; ++j) {
            total += matrix->data[i * matrix->cols + j] * vector[j];
        }
        product[i] = total;
        if (!isfinite(total)) {
            return CDC_LINALG_NONFINITE;
        }
    }
    return CDC_LINALG_OK;
}

int cdc_matrix_all_finite(const cdc_matrix *matrix) {
    size_t i;
    if (!matrix_shape_valid(matrix)) {
        return 0;
    }
    for (i = 0; i < matrix->rows * matrix->cols; ++i) {
        if (!isfinite(matrix->data[i])) {
            return 0;
        }
    }
    return 1;
}

double cdc_matrix_max_abs(const cdc_matrix *matrix) {
    size_t i;
    double largest = 0.0;
    if (!matrix_shape_valid(matrix)) {
        return NAN;
    }
    for (i = 0; i < matrix->rows * matrix->cols; ++i) {
        double magnitude = fabs(matrix->data[i]);
        if (magnitude > largest) {
            largest = magnitude;
        }
    }
    return largest;
}

#ifdef CDC_U2_LAPACK_DGEES
static int dgees_select_none(double *real, double *imaginary) {
    (void)real;
    (void)imaginary;
    return 0;
}

static cdc_linalg_status system_dgees_compute(
    const cdc_matrix *input, cdc_matrix *q, cdc_matrix *t,
    double *eigen_real, double *eigen_imag, void *context) {
    size_t row;
    size_t column;
    int n;
    int lda;
    int ldvs;
    int sdim = 0;
    int info = 0;
    int lwork = -1;
    int *bwork = NULL;
    double *a_column_major = NULL;
    double *q_column_major = NULL;
    double work_query = 0.0;
    double *work = NULL;
    char jobvs = 'V';
    char sort = 'N';
    (void)context;
    if (!matrix_shape_valid(input) || !matrix_shape_valid(q) ||
        !matrix_shape_valid(t) || input->rows != input->cols ||
        q->rows != input->rows || q->cols != input->cols ||
        t->rows != input->rows || t->cols != input->cols ||
        !eigen_real || !eigen_imag || input->rows > (size_t)INT_MAX) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    n = (int)input->rows;
    lda = n;
    ldvs = n;
    a_column_major = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    q_column_major = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    bwork = (int *)calloc((size_t)n, sizeof(int));
    if (!a_column_major || !q_column_major || !bwork) {
        free(a_column_major);
        free(q_column_major);
        free(bwork);
        return CDC_LINALG_ALLOCATION_FAILED;
    }
    for (row = 0; row < input->rows; ++row) {
        for (column = 0; column < input->cols; ++column) {
            a_column_major[column * input->rows + row] =
                input->data[row * input->cols + column];
        }
    }
    dgees_(&jobvs, &sort, dgees_select_none, &n, a_column_major, &lda,
           &sdim, eigen_real, eigen_imag, q_column_major, &ldvs,
           &work_query, &lwork, bwork, &info);
    if (info != 0 || !isfinite(work_query) || work_query < 1.0 ||
        work_query > (double)INT_MAX) {
        free(a_column_major);
        free(q_column_major);
        free(bwork);
        return CDC_LINALG_BACKEND_FAILED;
    }
    lwork = (int)ceil(work_query);
    work = (double *)malloc((size_t)lwork * sizeof(double));
    if (!work) {
        free(a_column_major);
        free(q_column_major);
        free(bwork);
        return CDC_LINALG_ALLOCATION_FAILED;
    }
    dgees_(&jobvs, &sort, dgees_select_none, &n, a_column_major, &lda,
           &sdim, eigen_real, eigen_imag, q_column_major, &ldvs,
           work, &lwork, bwork, &info);
    if (info != 0) {
        free(a_column_major);
        free(q_column_major);
        free(bwork);
        free(work);
        return CDC_LINALG_BACKEND_FAILED;
    }
    for (row = 0; row < input->rows; ++row) {
        for (column = 0; column < input->cols; ++column) {
            t->data[row * t->cols + column] =
                a_column_major[column * input->rows + row];
            q->data[row * q->cols + column] =
                q_column_major[column * input->rows + row];
        }
    }
    free(a_column_major);
    free(q_column_major);
    free(bwork);
    free(work);
    return CDC_LINALG_OK;
}
#endif

const cdc_real_schur_backend *cdc_system_real_schur_backend(void) {
#ifdef CDC_U2_LAPACK_DGEES
    static const cdc_real_schur_backend backend = {
        "lapack-dgees", system_dgees_compute, NULL
    };
    return &backend;
#else
    return NULL;
#endif
}

void cdc_real_schur_result_release(cdc_real_schur_result *result) {
    if (!result) {
        return;
    }
    cdc_matrix_release(&result->q);
    cdc_matrix_release(&result->t);
    free(result->eigen_real);
    free(result->eigen_imag);
    memset(result, 0, sizeof(*result));
}

static cdc_linalg_status schur_residuals(
    const cdc_matrix *input, const cdc_matrix *q, const cdc_matrix *t,
    double *reconstruction, double *orthogonality, double *triangular) {
    size_t i;
    size_t j;
    size_t k;
    double *qt;
    double recon_max = 0.0;
    double ortho_max = 0.0;
    double triangular_max = 0.0;
    double scale = fmax(1.0, cdc_matrix_max_abs(input));
    qt = (double *)calloc(input->rows * input->cols, sizeof(double));
    if (!qt) {
        return CDC_LINALG_ALLOCATION_FAILED;
    }
    /* Cache Q*T once.  Recomputing it inside every reconstructed entry would
     * make validation O(n^4), which is unacceptable at the full state cap. */
    for (i = 0; i < input->rows; ++i) {
        for (k = 0; k < input->cols; ++k) {
            size_t m;
            for (m = 0; m < input->rows; ++m) {
                qt[i * input->cols + k] +=
                    q->data[i * q->cols + m] * t->data[m * t->cols + k];
            }
        }
    }
    for (i = 0; i < input->rows; ++i) {
        for (j = 0; j < input->cols; ++j) {
            double qtq = 0.0;
            double reconstructed = 0.0;
            for (k = 0; k < input->rows; ++k) {
                qtq += q->data[k * q->cols + i] * q->data[k * q->cols + j];
                reconstructed += qt[i * input->cols + k] *
                                 q->data[j * q->cols + k];
            }
            if (fabs(qtq - (i == j ? 1.0 : 0.0)) > ortho_max) {
                ortho_max = fabs(qtq - (i == j ? 1.0 : 0.0));
            }
            if (fabs(reconstructed - input->data[i * input->cols + j]) > recon_max) {
                recon_max = fabs(reconstructed - input->data[i * input->cols + j]);
            }
            if (i > j + 1 && fabs(t->data[i * t->cols + j]) > triangular_max) {
                triangular_max = fabs(t->data[i * t->cols + j]);
            }
        }
    }
    *reconstruction = recon_max / scale;
    *orthogonality = ortho_max;
    *triangular = triangular_max / scale;
    if (!isfinite(*reconstruction) || !isfinite(*orthogonality) ||
        !isfinite(*triangular)) {
        free(qt);
        return CDC_LINALG_NONFINITE;
    }
    free(qt);
    return CDC_LINALG_OK;
}

cdc_linalg_status cdc_real_schur_compute(
    const cdc_matrix *input, const cdc_real_schur_backend *requested_backend,
    double validation_tolerance, cdc_real_schur_result *result) {
    size_t i;
    cdc_linalg_status status;
    const cdc_real_schur_backend *backend = requested_backend;
    if (!matrix_shape_valid(input) || input->rows != input->cols || !result ||
        !isfinite(validation_tolerance) || validation_tolerance <= 0.0) {
        return CDC_LINALG_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (!backend) {
        backend = cdc_system_real_schur_backend();
    }
    if (!backend || !backend->compute) {
        return CDC_LINALG_BACKEND_UNAVAILABLE;
    }
    result->dimension = input->rows;
    status = cdc_matrix_init(&result->q, input->rows, input->cols);
    if (status != CDC_LINALG_OK) {
        goto failure;
    }
    status = cdc_matrix_init(&result->t, input->rows, input->cols);
    if (status != CDC_LINALG_OK) {
        goto failure;
    }
    result->eigen_real = (double *)calloc(input->rows, sizeof(double));
    result->eigen_imag = (double *)calloc(input->rows, sizeof(double));
    if (!result->eigen_real || !result->eigen_imag) {
        status = CDC_LINALG_ALLOCATION_FAILED;
        goto failure;
    }
    status = backend->compute(input, &result->q, &result->t,
                              result->eigen_real, result->eigen_imag,
                              backend->context);
    if (status != CDC_LINALG_OK) {
        goto failure;
    }
    if (!cdc_matrix_all_finite(&result->q) ||
        !cdc_matrix_all_finite(&result->t)) {
        status = CDC_LINALG_NONFINITE;
        goto failure;
    }
    for (i = 0; i < input->rows; ++i) {
        if (!isfinite(result->eigen_real[i]) ||
            !isfinite(result->eigen_imag[i])) {
            status = CDC_LINALG_NONFINITE;
            goto failure;
        }
    }
    status = schur_residuals(input, &result->q, &result->t,
                             &result->reconstruction_residual,
                             &result->orthogonality_residual,
                             &result->triangular_residual);
    if (status != CDC_LINALG_OK) {
        goto failure;
    }
    if (result->reconstruction_residual > validation_tolerance ||
        result->orthogonality_residual > validation_tolerance ||
        result->triangular_residual > validation_tolerance) {
        status = CDC_LINALG_VALIDATION_FAILED;
        goto failure;
    }
    if (snprintf(result->backend, sizeof(result->backend), "%s",
                 backend->name ? backend->name : "unnamed") < 0) {
        status = CDC_LINALG_BACKEND_FAILED;
        goto failure;
    }
    return CDC_LINALG_OK;

failure:
    cdc_real_schur_result_release(result);
    return status;
}
