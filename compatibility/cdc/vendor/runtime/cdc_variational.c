#include "cdc_variational.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CDC_U2_PI 3.14159265358979323846264338327950288
#define CDC_U2_TWO_PI (2.0 * CDC_U2_PI)

static int finite_vector(const double *values, size_t count) {
    size_t i;
    if (!values) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (!isfinite(values[i])) {
            return 0;
        }
    }
    return 1;
}

static int square_matrix(const cdc_matrix *matrix, size_t dimension) {
    return matrix && matrix->data && matrix->rows == dimension &&
           matrix->cols == dimension;
}

static cdc_u2_status from_linalg(cdc_linalg_status status) {
    switch (status) {
    case CDC_LINALG_OK:
        return CDC_U2_OK;
    case CDC_LINALG_INVALID_ARGUMENT:
        return CDC_U2_INVALID_ARGUMENT;
    case CDC_LINALG_DIMENSION_MISMATCH:
        return CDC_U2_DIMENSION_MISMATCH;
    case CDC_LINALG_ALLOCATION_FAILED:
        return CDC_U2_ALLOCATION_FAILED;
    case CDC_LINALG_NONFINITE:
        return CDC_U2_NONFINITE;
    case CDC_LINALG_BACKEND_UNAVAILABLE:
        return CDC_U2_SPECTRAL_BACKEND_UNAVAILABLE;
    case CDC_LINALG_BACKEND_FAILED:
        return CDC_U2_SPECTRAL_BACKEND_FAILED;
    case CDC_LINALG_VALIDATION_FAILED:
        return CDC_U2_VALIDATION_FAILED;
    }
    return CDC_U2_VALIDATION_FAILED;
}

const char *cdc_u2_status_reason(cdc_u2_status status) {
    switch (status) {
    case CDC_U2_OK:
        return "none";
    case CDC_U2_INVALID_ARGUMENT:
        return "invalid-argument";
    case CDC_U2_DIMENSION_MISMATCH:
        return "dimension-mismatch";
    case CDC_U2_ALLOCATION_FAILED:
        return "allocation-failed";
    case CDC_U2_NONFINITE:
        return "nonfinite-variational-state";
    case CDC_U2_MODE_DIVERGENCE:
        return "recurrence-mode-mismatch";
    case CDC_U2_GUARD_NOT_LOCALIZED:
        return "guard-event-not-localized";
    case CDC_U2_NONTRANSVERSE_EVENT:
        return "event-not-transverse";
    case CDC_U2_NOT_RECURRENT:
        return "recurrence-residual";
    case CDC_U2_RELATIVE_RESTORATION_UNVERIFIED:
        return "undeclared-quotient";
    case CDC_U2_NEUTRAL_MODE_UNVERIFIED:
        return "neutral-mode-unverified";
    case CDC_U2_SPECTRAL_BACKEND_UNAVAILABLE:
        return "spectral-backend-unavailable";
    case CDC_U2_SPECTRAL_BACKEND_FAILED:
        return "spectral-backend-failed";
    case CDC_U2_VALIDATION_FAILED:
        return "variational-validation-failed";
    }
    return "unknown-variational-failure";
}

static int layout_valid(const cdc_u2_layout *layout) {
    return layout && layout->dimension > 0 && layout->coordinates &&
           layout->dimension == layout->cell_count + 2 * layout->module_count;
}

static cdc_u2_status set_coordinate(cdc_u2_coordinate *coordinate,
                                    cdc_u2_coordinate_kind kind,
                                    size_t source_index,
                                    double period,
                                    const char *owner,
                                    const char *field) {
    int written;
    if (!coordinate || !owner || !field || owner[0] == '\0') {
        return CDC_U2_INVALID_ARGUMENT;
    }
    written = snprintf(coordinate->name, sizeof(coordinate->name), "%s.%s",
                       owner, field);
    if (written < 0 || (size_t)written >= sizeof(coordinate->name)) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    coordinate->kind = kind;
    coordinate->source_index = source_index;
    coordinate->period = period;
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_layout_build(
    cdc_u2_layout *layout, const char *const *cell_names, size_t cell_count,
    const char *const *module_names, size_t module_count) {
    size_t i;
    size_t cursor = 0;
    size_t dimension;
    cdc_u2_status status;
    if (!layout || (cell_count && !cell_names) ||
        (module_count && !module_names) ||
        module_count > (SIZE_MAX - cell_count) / 2) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    dimension = cell_count + 2 * module_count;
    if (dimension == 0 || dimension > SIZE_MAX / sizeof(cdc_u2_coordinate)) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    memset(layout, 0, sizeof(*layout));
    layout->coordinates = (cdc_u2_coordinate *)calloc(
        dimension, sizeof(cdc_u2_coordinate));
    if (!layout->coordinates) {
        return CDC_U2_ALLOCATION_FAILED;
    }
    layout->dimension = dimension;
    layout->cell_count = cell_count;
    layout->module_count = module_count;
    for (i = 0; i < cell_count; ++i) {
        status = set_coordinate(&layout->coordinates[cursor++],
                                CDC_U2_COORDINATE_THETA, i, CDC_U2_TWO_PI,
                                cell_names[i], "theta");
        if (status != CDC_U2_OK) {
            cdc_u2_layout_release(layout);
            return status;
        }
    }
    for (i = 0; i < module_count; ++i) {
        status = set_coordinate(&layout->coordinates[cursor++],
                                CDC_U2_COORDINATE_BELIEF, i, 0.0,
                                module_names[i], "belief");
        if (status == CDC_U2_OK) {
            status = set_coordinate(&layout->coordinates[cursor++],
                                    CDC_U2_COORDINATE_PRIOR, i, 0.0,
                                    module_names[i], "prior");
        }
        if (status != CDC_U2_OK) {
            cdc_u2_layout_release(layout);
            return status;
        }
    }
    return CDC_U2_OK;
}

void cdc_u2_layout_release(cdc_u2_layout *layout) {
    if (!layout) {
        return;
    }
    free(layout->coordinates);
    memset(layout, 0, sizeof(*layout));
}

long cdc_u2_layout_find(const cdc_u2_layout *layout, const char *name) {
    size_t i;
    if (!layout_valid(layout) || !name) {
        return -1;
    }
    for (i = 0; i < layout->dimension; ++i) {
        if (strcmp(layout->coordinates[i].name, name) == 0) {
            return (long)i;
        }
    }
    return -1;
}

cdc_u2_status cdc_u2_layout_set_period(cdc_u2_layout *layout,
                                       size_t coordinate, double period) {
    if (!layout_valid(layout) || coordinate >= layout->dimension ||
        !isfinite(period) || period < 0.0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    layout->coordinates[coordinate].period = period;
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_state_pack(
    const cdc_u2_layout *layout, const double *theta, size_t theta_count,
    const double *belief, const double *prior, size_t module_count,
    double *packed, size_t packed_count) {
    size_t i;
    size_t cursor = 0;
    if (!layout_valid(layout) || !packed || packed_count != layout->dimension ||
        theta_count != layout->cell_count || module_count != layout->module_count ||
        (theta_count && !theta) || (module_count && (!belief || !prior))) {
        return CDC_U2_DIMENSION_MISMATCH;
    }
    if (!finite_vector(theta, theta_count) ||
        !finite_vector(belief, module_count) ||
        !finite_vector(prior, module_count)) {
        return CDC_U2_NONFINITE;
    }
    for (i = 0; i < theta_count; ++i) {
        packed[cursor++] = theta[i];
    }
    for (i = 0; i < module_count; ++i) {
        packed[cursor++] = belief[i];
        packed[cursor++] = prior[i];
    }
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_state_unpack(
    const cdc_u2_layout *layout, const double *packed, size_t packed_count,
    double *theta, size_t theta_count, double *belief, double *prior,
    size_t module_count) {
    size_t i;
    size_t cursor = 0;
    if (!layout_valid(layout) || !packed || packed_count != layout->dimension ||
        theta_count != layout->cell_count || module_count != layout->module_count ||
        (theta_count && !theta) || (module_count && (!belief || !prior))) {
        return CDC_U2_DIMENSION_MISMATCH;
    }
    if (!finite_vector(packed, packed_count)) {
        return CDC_U2_NONFINITE;
    }
    for (i = 0; i < theta_count; ++i) {
        theta[i] = packed[cursor++];
    }
    for (i = 0; i < module_count; ++i) {
        belief[i] = packed[cursor++];
        prior[i] = packed[cursor++];
    }
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_flow_jacobian(
    const cdc_u2_layout *layout, const double *state, size_t state_count,
    const cdc_u2_flow_coupling *couplings, size_t coupling_count,
    cdc_matrix *jacobian) {
    size_t i;
    cdc_linalg_status matrix_status;
    if (!layout_valid(layout) || !state || state_count != layout->dimension ||
        !square_matrix(jacobian, layout->dimension) ||
        (coupling_count && !couplings)) {
        return CDC_U2_DIMENSION_MISMATCH;
    }
    if (!finite_vector(state, state_count)) {
        return CDC_U2_NONFINITE;
    }
    matrix_status = cdc_matrix_identity(jacobian);
    if (matrix_status != CDC_LINALG_OK) {
        return from_linalg(matrix_status);
    }
    for (i = 0; i < coupling_count; ++i) {
        const cdc_u2_flow_coupling *coupling = &couplings[i];
        double slope;
        if (coupling->source_theta >= layout->dimension ||
            coupling->target_theta >= layout->dimension ||
            layout->coordinates[coupling->source_theta].kind != CDC_U2_COORDINATE_THETA ||
            layout->coordinates[coupling->target_theta].kind != CDC_U2_COORDINATE_THETA) {
            return CDC_U2_DIMENSION_MISMATCH;
        }
        if (!isfinite(coupling->coefficient) || !isfinite(coupling->angle)) {
            return CDC_U2_NONFINITE;
        }
        slope = coupling->coefficient *
                cos(state[coupling->source_theta] + coupling->angle -
                    state[coupling->target_theta]);
        jacobian->data[coupling->target_theta * jacobian->cols +
                       coupling->source_theta] += slope;
        jacobian->data[coupling->target_theta * jacobian->cols +
                       coupling->target_theta] -= slope;
    }
    return cdc_matrix_all_finite(jacobian) ? CDC_U2_OK : CDC_U2_NONFINITE;
}

cdc_u2_status cdc_u2_nest_jacobian(
    size_t dimension, size_t parent_belief, size_t child_belief,
    size_t child_prior, double gain, cdc_matrix *jacobian) {
    size_t column;
    cdc_linalg_status status;
    if (!square_matrix(jacobian, dimension) || parent_belief >= dimension ||
        child_belief >= dimension || child_prior >= dimension ||
        !isfinite(gain)) {
        return CDC_U2_DIMENSION_MISMATCH;
    }
    status = cdc_matrix_identity(jacobian);
    if (status != CDC_LINALG_OK) {
        return from_linalg(status);
    }
#ifdef CDC_U2_MUTANT_THEORETICAL_NEST_DERIVATIVE
    /* Deliberate test mutant: differentiates a theory expression that the
     * executable runtime does not evaluate. */
    jacobian->data[parent_belief * dimension + child_belief] += gain;
    jacobian->data[parent_belief * dimension + child_prior] -= gain;
#endif
    for (column = 0; column < dimension; ++column) {
        jacobian->data[child_prior * dimension + column] =
            jacobian->data[parent_belief * dimension + column];
    }
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_commit_jacobian(size_t dimension,
                                     cdc_u2_commit_kind kind,
                                     int fixed_mode,
                                     cdc_matrix *jacobian) {
    if (!square_matrix(jacobian, dimension)) {
        return CDC_U2_DIMENSION_MISMATCH;
    }
    if (kind == CDC_U2_COMMIT_GUARD_TRIGGERED) {
        return CDC_U2_GUARD_NOT_LOCALIZED;
    }
    if (kind != CDC_U2_COMMIT_SCHEDULED) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    if (!fixed_mode) {
        return CDC_U2_MODE_DIVERGENCE;
    }
    return from_linalg(cdc_matrix_identity(jacobian));
}

cdc_u2_status cdc_u2_saltation_matrix(
    const cdc_matrix *reset_jacobian, const double *flow_before,
    const double *flow_after, const double *guard_normal,
    const double *reset_time_derivative, size_t dimension,
    double guard_time_derivative, double transversality_tolerance,
    cdc_matrix *saltation, double *denominator_out) {
    size_t i;
#ifndef CDC_U2_MUTANT_OMIT_SALTATION
    size_t j;
#endif
    double denominator = guard_time_derivative;
    double *reset_flow = NULL;
    cdc_linalg_status linear_status;
    if (!square_matrix(reset_jacobian, dimension) ||
        !square_matrix(saltation, dimension) || !flow_before || !flow_after ||
        !guard_normal || !isfinite(guard_time_derivative) ||
        !isfinite(transversality_tolerance) || transversality_tolerance <= 0.0 ||
        !finite_vector(flow_before, dimension) ||
        !finite_vector(flow_after, dimension) ||
        !finite_vector(guard_normal, dimension) ||
        (reset_time_derivative &&
         !finite_vector(reset_time_derivative, dimension))) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    reset_flow = (double *)calloc(dimension, sizeof(double));
    if (!reset_flow) {
        return CDC_U2_ALLOCATION_FAILED;
    }
    linear_status = cdc_matrix_jvp(reset_jacobian, flow_before, dimension,
                                   reset_flow, dimension);
    if (linear_status != CDC_LINALG_OK) {
        free(reset_flow);
        return from_linalg(linear_status);
    }
    for (i = 0; i < dimension; ++i) {
        denominator += guard_normal[i] * flow_before[i];
    }
    if (denominator_out) {
        *denominator_out = denominator;
    }
    if (!isfinite(denominator) || fabs(denominator) <= transversality_tolerance) {
        free(reset_flow);
        return CDC_U2_NONTRANSVERSE_EVENT;
    }
    linear_status = cdc_matrix_copy(saltation, reset_jacobian);
    if (linear_status != CDC_LINALG_OK) {
        free(reset_flow);
        return from_linalg(linear_status);
    }
#ifndef CDC_U2_MUTANT_OMIT_SALTATION
    for (i = 0; i < dimension; ++i) {
        double numerator = flow_after[i] - reset_flow[i] -
                           (reset_time_derivative ? reset_time_derivative[i] : 0.0);
        for (j = 0; j < dimension; ++j) {
            saltation->data[i * dimension + j] +=
                numerator * guard_normal[j] / denominator;
        }
    }
#endif
    free(reset_flow);
    return cdc_matrix_all_finite(saltation) ? CDC_U2_OK : CDC_U2_NONFINITE;
}

const char *cdc_u2_event_kind_name(cdc_u2_event_kind kind) {
    switch (kind) {
    case CDC_U2_EVENT_FLOW:
        return "flow";
    case CDC_U2_EVENT_SCHEDULED_COMMIT:
        return "scheduled-commit";
    case CDC_U2_EVENT_GUARD_TRANSITION:
        return "guard-transition";
    case CDC_U2_EVENT_NEST:
        return "nest";
    case CDC_U2_EVENT_ENDPOINT_RESTORATION:
        return "endpoint-restoration";
    }
    return "unknown";
}

cdc_u2_status cdc_u2_monodromy_init(cdc_u2_monodromy *monodromy,
                                    size_t dimension) {
    cdc_linalg_status status;
    if (!monodromy || dimension == 0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    memset(monodromy, 0, sizeof(*monodromy));
    status = cdc_matrix_init(&monodromy->matrix, dimension, dimension);
    if (status != CDC_LINALG_OK) {
        return from_linalg(status);
    }
    status = cdc_matrix_identity(&monodromy->matrix);
    if (status != CDC_LINALG_OK) {
        cdc_u2_monodromy_release(monodromy);
        return from_linalg(status);
    }
    monodromy->dimension = dimension;
    return CDC_U2_OK;
}

void cdc_u2_monodromy_release(cdc_u2_monodromy *monodromy) {
    if (!monodromy) {
        return;
    }
    cdc_matrix_release(&monodromy->matrix);
    free(monodromy->events);
    memset(monodromy, 0, sizeof(*monodromy));
}

cdc_u2_status cdc_u2_monodromy_append(
    cdc_u2_monodromy *monodromy, const cdc_matrix *local,
    cdc_u2_event_kind kind, double time, int transverse) {
    cdc_linalg_status status;
    cdc_u2_event *grown;
    size_t capacity;
    if (!monodromy || monodromy->dimension == 0 ||
        !square_matrix(&monodromy->matrix, monodromy->dimension) ||
        !square_matrix(local, monodromy->dimension) || !isfinite(time) ||
        transverse < -1 || transverse > 1 ||
        kind < CDC_U2_EVENT_FLOW || kind > CDC_U2_EVENT_ENDPOINT_RESTORATION) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    if (monodromy->event_count > 0 &&
        time < monodromy->events[monodromy->event_count - 1].time) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    if (kind == CDC_U2_EVENT_GUARD_TRANSITION && transverse != 1) {
        return CDC_U2_NONTRANSVERSE_EVENT;
    }
    if (monodromy->event_count == monodromy->event_capacity) {
        capacity = monodromy->event_capacity ? monodromy->event_capacity * 2 : 8;
        if (capacity < monodromy->event_capacity ||
            capacity > SIZE_MAX / sizeof(cdc_u2_event)) {
            return CDC_U2_ALLOCATION_FAILED;
        }
        grown = (cdc_u2_event *)realloc(monodromy->events,
                                        capacity * sizeof(cdc_u2_event));
        if (!grown) {
            return CDC_U2_ALLOCATION_FAILED;
        }
        monodromy->events = grown;
        monodromy->event_capacity = capacity;
    }
#ifdef CDC_U2_MUTANT_REVERSE_PRODUCT
    status = cdc_matrix_multiply(&monodromy->matrix, &monodromy->matrix, local);
#else
    status = cdc_matrix_multiply(&monodromy->matrix, local, &monodromy->matrix);
#endif
    if (status != CDC_LINALG_OK) {
        return from_linalg(status);
    }
    monodromy->events[monodromy->event_count].kind = kind;
    monodromy->events[monodromy->event_count].time = time;
    monodromy->events[monodromy->event_count].transverse = transverse;
    monodromy->event_count++;
    return CDC_U2_OK;
}

const char *cdc_u2_recurrence_kind_name(cdc_u2_recurrence_kind kind) {
    return kind == CDC_U2_RECURRENCE_RELATIVE ? "relative" : "full";
}

static double periodic_difference(double final, double initial, double period) {
    double difference = final - initial;
    if (period > 0.0) {
        difference = fmod(difference, period);
        if (difference > 0.5 * period) {
            difference -= period;
        } else if (difference < -0.5 * period) {
            difference += period;
        }
    }
    return difference;
}

cdc_u2_status cdc_u2_recurrence_check(
    const cdc_u2_layout *layout, const double *initial_state,
    const double *final_state, size_t state_count,
    const cdc_u2_recurrence_spec *spec,
    cdc_u2_recurrence_result *result) {
    size_t i;
    double *restored = NULL;
    const double *candidate = final_state;
    cdc_u2_status restore_status;
    double absolute_tolerance;
    double relative_tolerance;
    if (!layout_valid(layout) || !initial_state || !final_state || !spec ||
        !result || state_count != layout->dimension ||
        (spec->kind != CDC_U2_RECURRENCE_FULL &&
         spec->kind != CDC_U2_RECURRENCE_RELATIVE) ||
        !finite_vector(initial_state, state_count) ||
        !finite_vector(final_state, state_count)) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    absolute_tolerance = spec->absolute_tolerance > 0.0
                             ? spec->absolute_tolerance
                             : spec->tolerance;
    relative_tolerance = spec->relative_tolerance;
    if (!isfinite(absolute_tolerance) || absolute_tolerance <= 0.0 ||
        !isfinite(relative_tolerance) || relative_tolerance < 0.0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    result->kind = spec->kind;
    result->tolerance = absolute_tolerance;
    result->absolute_tolerance = absolute_tolerance;
    result->relative_tolerance = relative_tolerance;
    result->restoration_equivariant = spec->restoration_equivariant ? 1 : 0;
    result->discrete_state_verified = spec->discrete_state_verified ? 1 : 0;
    if (spec->kind == CDC_U2_RECURRENCE_RELATIVE) {
        if (!spec->restore_endpoint || !spec->restoration_equivariant) {
            return CDC_U2_RELATIVE_RESTORATION_UNVERIFIED;
        }
        if (!square_matrix(spec->restoration_derivative, state_count) ||
            !cdc_matrix_all_finite(spec->restoration_derivative)) {
            return CDC_U2_RELATIVE_RESTORATION_UNVERIFIED;
        }
        restored = (double *)calloc(state_count, sizeof(double));
        if (!restored) {
            return CDC_U2_ALLOCATION_FAILED;
        }
        restore_status = spec->restore_endpoint(final_state, state_count,
                                                restored,
                                                spec->restoration_context);
        if (restore_status != CDC_U2_OK || !finite_vector(restored, state_count)) {
            free(restored);
            return restore_status == CDC_U2_OK ? CDC_U2_NONFINITE : restore_status;
        }
        candidate = restored;
        result->restoration_applied = 1;
        result->restoration_derivative_bound = 1;
    }
    for (i = 0; i < state_count; ++i) {
        double residual;
        double weight = spec->weights ? spec->weights[i] : 1.0;
        double allowed;
        double normalized;
        if (spec->include && !spec->include[i]) {
            result->projected = 1;
            continue;
        }
        if (!isfinite(weight) || weight <= 0.0) {
            free(restored);
            return CDC_U2_INVALID_ARGUMENT;
        }
        result->included_coordinates++;
        residual = fabs(periodic_difference(candidate[i], initial_state[i],
                                            layout->coordinates[i].period)) * weight;
        allowed = absolute_tolerance + relative_tolerance * weight *
            fmax(fabs(candidate[i]), fabs(initial_state[i]));
        normalized = residual / allowed;
        if (residual > result->residual) {
            result->residual = residual;
        }
        if (allowed > result->maximum_allowed_residual) {
            result->maximum_allowed_residual = allowed;
        }
        if (normalized > result->normalized_residual) {
            result->normalized_residual = normalized;
        }
    }
    free(restored);
    if (result->included_coordinates == 0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    result->verified = result->normalized_residual <= 1.0 &&
                       result->discrete_state_verified;
    result->authorizes_monodromy = result->verified && !result->projected &&
                                   spec->kind == CDC_U2_RECURRENCE_FULL;
#ifdef CDC_U2_MUTANT_ACCEPT_FALSE_RECURRENCE
    result->verified = 1;
    result->authorizes_monodromy = 1;
#endif
    if (!result->discrete_state_verified) {
        return CDC_U2_MODE_DIVERGENCE;
    }
    if (spec->kind == CDC_U2_RECURRENCE_RELATIVE && result->verified &&
        !result->projected) {
        return CDC_U2_OK;
    }
    return result->authorizes_monodromy ? CDC_U2_OK : CDC_U2_NOT_RECURRENT;
}

cdc_u2_status cdc_u2_monodromy_bind_recurrence(
    cdc_u2_monodromy *monodromy, const cdc_u2_recurrence_spec *spec,
    cdc_u2_recurrence_result *result, double endpoint_time) {
    cdc_u2_status status;
    if (!monodromy || !spec || !result || !isfinite(endpoint_time) ||
        monodromy->dimension == 0 || result->kind != spec->kind ||
        !result->verified || result->projected ||
        !result->discrete_state_verified) {
        return CDC_U2_NOT_RECURRENT;
    }
    if (spec->kind == CDC_U2_RECURRENCE_FULL) {
        result->authorizes_monodromy = 1;
        return CDC_U2_OK;
    }
    if (!result->restoration_applied || !result->restoration_equivariant ||
        !result->restoration_derivative_bound ||
        !square_matrix(spec->restoration_derivative, monodromy->dimension) ||
        !cdc_matrix_all_finite(spec->restoration_derivative)) {
        return CDC_U2_RELATIVE_RESTORATION_UNVERIFIED;
    }
#ifdef CDC_U2_MUTANT_OMIT_RESTORATION_DERIVATIVE
    status = CDC_U2_OK;
#else
    status = cdc_u2_monodromy_append(
        monodromy, spec->restoration_derivative,
        CDC_U2_EVENT_ENDPOINT_RESTORATION, endpoint_time, -1);
#endif
    if (status != CDC_U2_OK) {
        return status;
    }
    result->restoration_derivative_applied = 1;
    result->authorizes_monodromy = 1;
    return CDC_U2_OK;
}

const char *cdc_u2_stability_class_name(cdc_u2_stability_class classification) {
    switch (classification) {
    case CDC_U2_STABILITY_STABLE:
        return "stable";
    case CDC_U2_STABILITY_MARGINAL:
        return "marginal";
    case CDC_U2_STABILITY_UNSTABLE:
        return "unstable";
    }
    return "unknown";
}

void cdc_u2_spectrum_release(cdc_u2_spectrum *spectrum) {
    if (!spectrum) {
        return;
    }
    free(spectrum->multipliers);
    memset(spectrum, 0, sizeof(*spectrum));
}

static int multiplier_before(const cdc_u2_multiplier *left,
                             const cdc_u2_multiplier *right) {
    if (left->modulus != right->modulus) {
        return left->modulus > right->modulus;
    }
    if (left->real != right->real) {
        return left->real > right->real;
    }
    return left->imag > right->imag;
}

static void sort_multipliers(cdc_u2_multiplier *multipliers, size_t count) {
    size_t i;
    for (i = 1; i < count; ++i) {
        cdc_u2_multiplier value = multipliers[i];
        size_t j = i;
        while (j > 0 && multiplier_before(&value, &multipliers[j - 1])) {
            multipliers[j] = multipliers[j - 1];
            --j;
        }
        multipliers[j] = value;
    }
}

static cdc_u2_status validate_gauge_generators(
    const cdc_matrix *monodromy, const cdc_matrix *generators,
    double tolerance) {
    size_t n;
    size_t k;
    size_t column;
    size_t prior;
    size_t row;
    double *orthogonal = NULL;
    double *product = NULL;
    if (!generators) {
        return CDC_U2_OK;
    }
    n = monodromy->rows;
    k = generators->cols;
    if (!generators->data || generators->rows != n || k == 0 || k > n) {
        return CDC_U2_NEUTRAL_MODE_UNVERIFIED;
    }
    orthogonal = (double *)calloc(n * k, sizeof(double));
    product = (double *)calloc(n, sizeof(double));
    if (!orthogonal || !product) {
        free(orthogonal);
        free(product);
        return CDC_U2_ALLOCATION_FAILED;
    }
    for (column = 0; column < k; ++column) {
        double norm_squared = 0.0;
        double original_norm_squared = 0.0;
        double residual = 0.0;
        for (row = 0; row < n; ++row) {
            double value = generators->data[row * k + column];
            if (!isfinite(value)) {
                free(orthogonal);
                free(product);
                return CDC_U2_NEUTRAL_MODE_UNVERIFIED;
            }
            orthogonal[row * k + column] = value;
            original_norm_squared += value * value;
        }
        for (prior = 0; prior < column; ++prior) {
            double projection = 0.0;
            for (row = 0; row < n; ++row) {
                projection += orthogonal[row * k + column] *
                              orthogonal[row * k + prior];
            }
            for (row = 0; row < n; ++row) {
                orthogonal[row * k + column] -=
                    projection * orthogonal[row * k + prior];
            }
        }
        for (row = 0; row < n; ++row) {
            double value = orthogonal[row * k + column];
            norm_squared += value * value;
        }
        if (!isfinite(norm_squared) || norm_squared <= tolerance * tolerance ||
            original_norm_squared <= tolerance * tolerance) {
            free(orthogonal);
            free(product);
            return CDC_U2_NEUTRAL_MODE_UNVERIFIED;
        }
        {
            double inverse_norm = 1.0 / sqrt(norm_squared);
            for (row = 0; row < n; ++row) {
                orthogonal[row * k + column] *= inverse_norm;
            }
        }
        for (row = 0; row < n; ++row) {
            size_t inner;
            double mapped = 0.0;
            for (inner = 0; inner < n; ++inner) {
                mapped += monodromy->data[row * n + inner] *
                          generators->data[inner * k + column];
            }
            product[row] = mapped;
            if (fabs(mapped - generators->data[row * k + column]) > residual) {
                residual = fabs(mapped - generators->data[row * k + column]);
            }
        }
        if (residual / fmax(1.0, sqrt(original_norm_squared)) > tolerance) {
            free(orthogonal);
            free(product);
            return CDC_U2_NEUTRAL_MODE_UNVERIFIED;
        }
    }
    free(orthogonal);
    free(product);
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_spectrum_compute(
    const cdc_matrix *monodromy, const cdc_real_schur_backend *backend,
    const cdc_u2_recurrence_result *recurrence,
    const cdc_matrix *gauge_generators, double neutral_tolerance,
    double schur_validation_tolerance, cdc_u2_spectrum *spectrum) {
    size_t i;
    size_t gauge_count = gauge_generators ? gauge_generators->cols : 0;
    cdc_real_schur_result schur;
    cdc_linalg_status linear_status;
    cdc_u2_status gauge_status;
    int saw_marginal = 0;
    if (!monodromy || !monodromy->data || monodromy->rows == 0 ||
        monodromy->rows != monodromy->cols || !recurrence || !spectrum ||
        !isfinite(neutral_tolerance) || neutral_tolerance <= 0.0 ||
        !isfinite(schur_validation_tolerance) ||
        schur_validation_tolerance <= 0.0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    if (!recurrence->authorizes_monodromy) {
        return CDC_U2_NOT_RECURRENT;
    }
    memset(spectrum, 0, sizeof(*spectrum));
    gauge_status = validate_gauge_generators(monodromy, gauge_generators,
                                             neutral_tolerance);
    if (gauge_status != CDC_U2_OK) {
        return gauge_status;
    }
    memset(&schur, 0, sizeof(schur));
    linear_status = cdc_real_schur_compute(monodromy, backend,
                                           schur_validation_tolerance, &schur);
    if (linear_status != CDC_LINALG_OK) {
        return from_linalg(linear_status);
    }
    spectrum->multipliers = (cdc_u2_multiplier *)calloc(
        monodromy->rows, sizeof(cdc_u2_multiplier));
    if (!spectrum->multipliers) {
        cdc_real_schur_result_release(&schur);
        return CDC_U2_ALLOCATION_FAILED;
    }
    spectrum->dimension = monodromy->rows;
    spectrum->schur_reconstruction_residual = schur.reconstruction_residual;
    spectrum->schur_orthogonality_residual = schur.orthogonality_residual;
    spectrum->schur_triangular_residual = schur.triangular_residual;
    snprintf(spectrum->backend, sizeof(spectrum->backend), "%s", schur.backend);
    for (i = 0; i < spectrum->dimension; ++i) {
        spectrum->multipliers[i].real = schur.eigen_real[i];
        spectrum->multipliers[i].imag = schur.eigen_imag[i];
        spectrum->multipliers[i].modulus = hypot(schur.eigen_real[i],
                                                 schur.eigen_imag[i]);
        spectrum->multipliers[i].mode = CDC_U2_MULTIPLIER_PHYSICAL;
        if (spectrum->multipliers[i].modulus > spectrum->spectral_radius) {
            spectrum->spectral_radius = spectrum->multipliers[i].modulus;
        }
    }
    cdc_real_schur_result_release(&schur);
    sort_multipliers(spectrum->multipliers, spectrum->dimension);
    for (i = 0; i < gauge_count; ++i) {
        size_t candidate;
        size_t best = SIZE_MAX;
        double best_distance = HUGE_VAL;
        for (candidate = 0; candidate < spectrum->dimension; ++candidate) {
            double distance;
            if (spectrum->multipliers[candidate].mode == CDC_U2_MULTIPLIER_GAUGE) {
                continue;
            }
            distance = hypot(spectrum->multipliers[candidate].real - 1.0,
                             spectrum->multipliers[candidate].imag);
            if (distance < best_distance) {
                best_distance = distance;
                best = candidate;
            }
        }
        if (best == SIZE_MAX || best_distance > neutral_tolerance) {
            cdc_u2_spectrum_release(spectrum);
            return CDC_U2_NEUTRAL_MODE_UNVERIFIED;
        }
        spectrum->multipliers[best].mode = CDC_U2_MULTIPLIER_GAUGE;
    }
    spectrum->classification = CDC_U2_STABILITY_STABLE;
    for (i = 0; i < spectrum->dimension; ++i) {
        double modulus;
        if (spectrum->multipliers[i].mode == CDC_U2_MULTIPLIER_GAUGE) {
            continue;
        }
        modulus = spectrum->multipliers[i].modulus;
        if (modulus > 1.0 + neutral_tolerance) {
            spectrum->classification = CDC_U2_STABILITY_UNSTABLE;
            return CDC_U2_OK;
        }
        if (modulus >= 1.0 - neutral_tolerance) {
            saw_marginal = 1;
        }
    }
    if (saw_marginal) {
        spectrum->classification = CDC_U2_STABILITY_MARGINAL;
    }
    return CDC_U2_OK;
}

const char *cdc_u2_polarity_reason_name(cdc_u2_polarity_reason reason) {
    switch (reason) {
    case CDC_U2_POLARITY_NONE:
        return "none";
    case CDC_U2_POLARITY_MAP_NOT_INVOLUTIVE:
        return "polarity-map-not-involutive";
    case CDC_U2_POLARITY_APERTURE_NOT_FIXED:
        return "aperture-not-fixed";
    case CDC_U2_POLARITY_CLOSURE_NOT_COVARIANT:
        return "closure-not-covariant";
    case CDC_U2_POLARITY_TANGENT_NOT_COVARIANT:
        return "tangent-not-covariant";
    }
    return "unknown-polarity-failure";
}

static double normalized_vector_gap(const double *left, const double *right,
                                    size_t count) {
    size_t i;
    double gap = 0.0;
    double scale = 1.0;
    for (i = 0; i < count; ++i) {
        double difference = fabs(left[i] - right[i]);
        scale = fmax(scale, fmax(fabs(left[i]), fabs(right[i])));
        if (difference > gap) {
            gap = difference;
        }
    }
    return gap / scale;
}

static double normalized_matrix_gap(const cdc_matrix *left,
                                    const cdc_matrix *right) {
    size_t i;
    double gap = 0.0;
    double scale = 1.0;
    size_t count = left->rows * left->cols;
    for (i = 0; i < count; ++i) {
        double difference = fabs(left->data[i] - right->data[i]);
        scale = fmax(scale, fmax(fabs(left->data[i]), fabs(right->data[i])));
        if (difference > gap) {
            gap = difference;
        }
    }
    return gap / scale;
}

cdc_u2_status cdc_u2_polarity_covariance_check(
    const cdc_u2_polarity_spec *spec, cdc_u2_polarity_result *result) {
    size_t n;
    double *storage = NULL;
    double *source_polar = NULL;
    double *source_twice = NULL;
    double *terminal = NULL;
    double *target_polar = NULL;
    double *target_twice = NULL;
    double *conjugate_terminal = NULL;
    double *aperture_mapped = NULL;
    cdc_matrix tangent_left;
    cdc_matrix tangent_right;
    cdc_u2_status status = CDC_U2_OK;
    cdc_linalg_status matrix_status;
    if (!spec || !result || spec->dimension == 0 ||
        !isfinite(spec->tolerance) || spec->tolerance <= 0.0 ||
        !spec->source_state || !spec->aperture_state ||
        !spec->source_polarity || !spec->target_polarity ||
        !spec->primal_map || !spec->conjugate_primal_map ||
        !finite_vector(spec->source_state, spec->dimension) ||
        !finite_vector(spec->aperture_state, spec->dimension) ||
        !square_matrix(spec->source_polarity_derivative, spec->dimension) ||
        !square_matrix(spec->target_polarity_derivative, spec->dimension) ||
        !square_matrix(spec->primal_derivative, spec->dimension) ||
        !square_matrix(spec->conjugate_primal_derivative, spec->dimension) ||
        !cdc_matrix_all_finite(spec->source_polarity_derivative) ||
        !cdc_matrix_all_finite(spec->target_polarity_derivative) ||
        !cdc_matrix_all_finite(spec->primal_derivative) ||
        !cdc_matrix_all_finite(spec->conjugate_primal_derivative)) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    memset(&tangent_left, 0, sizeof(tangent_left));
    memset(&tangent_right, 0, sizeof(tangent_right));
    n = spec->dimension;
    if (n > SIZE_MAX / (7 * sizeof(double))) {
        return CDC_U2_ALLOCATION_FAILED;
    }
    storage = (double *)calloc(7 * n, sizeof(double));
    if (!storage) {
        return CDC_U2_ALLOCATION_FAILED;
    }
    source_polar = storage;
    source_twice = source_polar + n;
    terminal = source_twice + n;
    target_polar = terminal + n;
    target_twice = target_polar + n;
    conjugate_terminal = target_twice + n;
    aperture_mapped = conjugate_terminal + n;

#define CDC_U2_POLARITY_MAP(call)                 \
    do {                                          \
        status = (call);                          \
        if (status != CDC_U2_OK) {                \
            goto cleanup;                         \
        }                                         \
    } while (0)

    CDC_U2_POLARITY_MAP(spec->source_polarity(
        spec->source_state, n, source_polar,
        spec->source_polarity_context));
    CDC_U2_POLARITY_MAP(spec->source_polarity(
        source_polar, n, source_twice,
        spec->source_polarity_context));
    CDC_U2_POLARITY_MAP(spec->primal_map(
        spec->source_state, n, terminal, spec->primal_context));
    CDC_U2_POLARITY_MAP(spec->target_polarity(
        terminal, n, target_polar, spec->target_polarity_context));
    CDC_U2_POLARITY_MAP(spec->target_polarity(
        target_polar, n, target_twice,
        spec->target_polarity_context));
    CDC_U2_POLARITY_MAP(spec->conjugate_primal_map(
        source_polar, n, conjugate_terminal,
        spec->conjugate_primal_context));
    CDC_U2_POLARITY_MAP(spec->source_polarity(
        spec->aperture_state, n, aperture_mapped,
        spec->source_polarity_context));
#undef CDC_U2_POLARITY_MAP

    if (!finite_vector(source_polar, n) || !finite_vector(source_twice, n) ||
        !finite_vector(terminal, n) || !finite_vector(target_polar, n) ||
        !finite_vector(target_twice, n) ||
        !finite_vector(conjugate_terminal, n) ||
        !finite_vector(aperture_mapped, n)) {
        status = CDC_U2_NONFINITE;
        goto cleanup;
    }
    result->source_involution_residual = normalized_vector_gap(
        source_twice, spec->source_state, n);
    result->target_involution_residual = normalized_vector_gap(
        target_twice, terminal, n);
    result->aperture_fixed_residual = normalized_vector_gap(
        aperture_mapped, spec->aperture_state, n);
    result->closure_covariance_residual = normalized_vector_gap(
        target_polar, conjugate_terminal, n);

    matrix_status = cdc_matrix_init(&tangent_left, n, n);
    if (matrix_status != CDC_LINALG_OK) {
        status = from_linalg(matrix_status);
        goto cleanup;
    }
    matrix_status = cdc_matrix_init(&tangent_right, n, n);
    if (matrix_status != CDC_LINALG_OK) {
        status = from_linalg(matrix_status);
        goto cleanup;
    }
    matrix_status = cdc_matrix_multiply(
        &tangent_left, spec->target_polarity_derivative,
        spec->primal_derivative);
    if (matrix_status == CDC_LINALG_OK) {
        matrix_status = cdc_matrix_multiply(
            &tangent_right, spec->conjugate_primal_derivative,
            spec->source_polarity_derivative);
    }
    if (matrix_status != CDC_LINALG_OK) {
        status = from_linalg(matrix_status);
        goto cleanup;
    }
    result->tangent_covariance_residual = normalized_matrix_gap(
        &tangent_left, &tangent_right);

#ifdef CDC_U2_MUTANT_IGNORE_POLARITY_APERTURE
    result->aperture_fixed_residual = 0.0;
#endif
#ifdef CDC_U2_MUTANT_SKIP_POLARITY_TANGENT
    result->tangent_covariance_residual = 0.0;
#endif

    result->reason = CDC_U2_POLARITY_NONE;
    if (result->source_involution_residual > spec->tolerance ||
        result->target_involution_residual > spec->tolerance) {
        result->reason = CDC_U2_POLARITY_MAP_NOT_INVOLUTIVE;
    } else if (result->aperture_fixed_residual > spec->tolerance) {
        result->reason = CDC_U2_POLARITY_APERTURE_NOT_FIXED;
    } else if (result->closure_covariance_residual > spec->tolerance) {
        result->reason = CDC_U2_POLARITY_CLOSURE_NOT_COVARIANT;
    } else if (result->tangent_covariance_residual > spec->tolerance) {
        result->reason = CDC_U2_POLARITY_TANGENT_NOT_COVARIANT;
    }
    result->accepted = result->reason == CDC_U2_POLARITY_NONE;
#ifdef CDC_U2_MUTANT_ACCEPT_FALSE_POLARITY
    result->reason = CDC_U2_POLARITY_NONE;
    result->accepted = 1;
#endif

cleanup:
    cdc_matrix_release(&tangent_left);
    cdc_matrix_release(&tangent_right);
    free(storage);
    return status;
}

cdc_u2_status cdc_u2_validate_jacobian(
    cdc_u2_map_fn map, void *context, const double *point, size_t dimension,
    const cdc_matrix *analytic, double relative_step,
    double absolute_tolerance, double *max_error_out) {
    size_t column;
    size_t row;
    double *plus = NULL;
    double *minus = NULL;
    double *output_plus = NULL;
    double *output_minus = NULL;
    double max_error = 0.0;
    cdc_u2_status status = CDC_U2_OK;
    if (!map || !point || !square_matrix(analytic, dimension) ||
        !finite_vector(point, dimension) || !isfinite(relative_step) ||
        relative_step <= 0.0 || !isfinite(absolute_tolerance) ||
        absolute_tolerance <= 0.0) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    plus = (double *)malloc(dimension * sizeof(double));
    minus = (double *)malloc(dimension * sizeof(double));
    output_plus = (double *)malloc(dimension * sizeof(double));
    output_minus = (double *)malloc(dimension * sizeof(double));
    if (!plus || !minus || !output_plus || !output_minus) {
        status = CDC_U2_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (column = 0; column < dimension; ++column) {
        double step = relative_step * fmax(1.0, fabs(point[column]));
        memcpy(plus, point, dimension * sizeof(double));
        memcpy(minus, point, dimension * sizeof(double));
        plus[column] += step;
        minus[column] -= step;
        status = map(plus, dimension, output_plus, context);
        if (status != CDC_U2_OK) {
            goto cleanup;
        }
        status = map(minus, dimension, output_minus, context);
        if (status != CDC_U2_OK) {
            goto cleanup;
        }
        if (!finite_vector(output_plus, dimension) ||
            !finite_vector(output_minus, dimension)) {
            status = CDC_U2_NONFINITE;
            goto cleanup;
        }
        for (row = 0; row < dimension; ++row) {
            double numerical = (output_plus[row] - output_minus[row]) /
                               (2.0 * step);
            double error = fabs(numerical -
                                analytic->data[row * dimension + column]);
            if (error > max_error) {
                max_error = error;
            }
        }
    }
    if (max_error > absolute_tolerance) {
        status = CDC_U2_VALIDATION_FAILED;
    }

cleanup:
    free(plus);
    free(minus);
    free(output_plus);
    free(output_minus);
    if (max_error_out) {
        *max_error_out = max_error;
    }
    return status;
}

typedef struct {
    double coefficient;
    double angle;
} self_test_flow_context;

static cdc_u2_status self_test_flow_map(const double *input, size_t dimension,
                                        double *output, void *opaque) {
    self_test_flow_context *context = (self_test_flow_context *)opaque;
    if (dimension != 2 || !input || !output || !context) {
        return CDC_U2_INVALID_ARGUMENT;
    }
    output[0] = input[0];
    output[1] = input[1] + context->coefficient *
        sin(input[0] + context->angle - input[1]);
    return CDC_U2_OK;
}

static cdc_u2_status self_test_restore(const double *final_state,
                                       size_t dimension,
                                       double *restored_state,
                                       void *context) {
    double shift = *(const double *)context;
    size_t i;
    for (i = 0; i < dimension; ++i) {
        restored_state[i] = final_state[i] - shift;
    }
    return CDC_U2_OK;
}

cdc_u2_status cdc_u2_self_test(void) {
    const char *cell_names[] = {"a", "b"};
    const char *module_names[] = {"m0", "m1"};
    cdc_u2_layout layout;
    cdc_matrix local;
    cdc_matrix nest;
    cdc_matrix reset;
    cdc_matrix saltation;
    cdc_matrix order_a;
    cdc_matrix order_b;
    cdc_matrix order_expected;
    cdc_matrix restoration_derivative;
    cdc_u2_monodromy monodromy;
    cdc_u2_monodromy order_trace;
    cdc_u2_flow_coupling coupling;
    cdc_u2_recurrence_spec recurrence_spec;
    cdc_u2_recurrence_result recurrence;
    self_test_flow_context flow_context;
    double state[6] = {0.2, -0.4, 1.0, 2.0, 3.0, 4.0};
    double initial[6] = {0.2, -0.4, 1.0, 2.0, 3.0, 4.0};
    double final[6] = {0.7, 0.1, 1.5, 2.5, 3.5, 4.5};
    double shift = 0.5;
    double flow_before[2] = {1.0, 0.0};
    double flow_after[2] = {2.0, 0.0};
    double normal[2] = {1.0, 0.0};
    double error = 0.0;
    cdc_u2_status status;
    memset(&layout, 0, sizeof(layout));
    memset(&local, 0, sizeof(local));
    memset(&nest, 0, sizeof(nest));
    memset(&reset, 0, sizeof(reset));
    memset(&saltation, 0, sizeof(saltation));
    memset(&order_a, 0, sizeof(order_a));
    memset(&order_b, 0, sizeof(order_b));
    memset(&order_expected, 0, sizeof(order_expected));
    memset(&restoration_derivative, 0, sizeof(restoration_derivative));
    memset(&monodromy, 0, sizeof(monodromy));
    memset(&order_trace, 0, sizeof(order_trace));
    status = cdc_u2_layout_build(&layout, cell_names, 2, module_names, 2);
    if (status != CDC_U2_OK) {
        return status;
    }
    if (cdc_matrix_init(&local, 6, 6) != CDC_LINALG_OK ||
        cdc_matrix_init(&nest, 6, 6) != CDC_LINALG_OK ||
        cdc_matrix_init(&reset, 2, 2) != CDC_LINALG_OK ||
        cdc_matrix_init(&saltation, 2, 2) != CDC_LINALG_OK ||
        cdc_matrix_init(&order_a, 2, 2) != CDC_LINALG_OK ||
        cdc_matrix_init(&order_b, 2, 2) != CDC_LINALG_OK ||
        cdc_matrix_init(&order_expected, 2, 2) != CDC_LINALG_OK ||
        cdc_matrix_init(&restoration_derivative, 6, 6) != CDC_LINALG_OK) {
        status = CDC_U2_ALLOCATION_FAILED;
        goto cleanup;
    }
    coupling.source_theta = 0;
    coupling.target_theta = 1;
    coupling.coefficient = 0.3;
    coupling.angle = 0.1;
    status = cdc_u2_flow_jacobian(&layout, state, 6, &coupling, 1, &local);
    if (status != CDC_U2_OK) {
        goto cleanup;
    }
    flow_context.coefficient = coupling.coefficient;
    flow_context.angle = coupling.angle;
    {
        cdc_matrix phase_view;
        double phase_jacobian[4] = {
            local.data[0], local.data[1],
            local.data[6], local.data[7]
        };
        cdc_matrix_view(&phase_view, 2, 2, phase_jacobian);
        status = cdc_u2_validate_jacobian(self_test_flow_map, &flow_context,
                                          state, 2, &phase_view, 1e-6, 1e-7,
                                          &error);
        if (status != CDC_U2_OK) {
            goto cleanup;
        }
    }
    status = cdc_u2_nest_jacobian(6, 2, 4, 5, 3.0, &nest);
    if (status != CDC_U2_OK || nest.data[5 * 6 + 2] != 1.0 ||
        nest.data[5 * 6 + 4] != 0.0 || nest.data[5 * 6 + 5] != 0.0 ||
        nest.data[2 * 6 + 4] != 0.0 || nest.data[2 * 6 + 5] != 0.0) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    cdc_matrix_identity(&reset);
    status = cdc_u2_saltation_matrix(&reset, flow_before, flow_after, normal,
                                     NULL, 2, 0.0, 1e-12, &saltation, NULL);
    if (status != CDC_U2_OK || fabs(saltation.data[0] - 2.0) > 1e-12) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    status = cdc_u2_monodromy_init(&monodromy, 6);
    if (status != CDC_U2_OK) {
        goto cleanup;
    }
    status = cdc_u2_monodromy_append(&monodromy, &local,
                                     CDC_U2_EVENT_FLOW, 1.0, -1);
    if (status == CDC_U2_OK) {
        status = cdc_u2_monodromy_append(&monodromy, &nest,
                                         CDC_U2_EVENT_NEST, 1.0, -1);
    }
    if (status != CDC_U2_OK || monodromy.event_count != 2) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    /* A and B are intentionally non-commuting, so reversing append order is
     * observable rather than hidden by the block structure of this fixture. */
    order_a.data[0] = 1.0;
    order_a.data[1] = 2.0;
    order_a.data[2] = 0.0;
    order_a.data[3] = 1.0;
    order_b.data[0] = 1.0;
    order_b.data[1] = 0.0;
    order_b.data[2] = 3.0;
    order_b.data[3] = 1.0;
    if (cdc_matrix_multiply(&order_expected, &order_b, &order_a) != CDC_LINALG_OK) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    status = cdc_u2_monodromy_init(&order_trace, 2);
    if (status == CDC_U2_OK) {
        status = cdc_u2_monodromy_append(&order_trace, &order_a,
                                         CDC_U2_EVENT_FLOW, 0.5, -1);
    }
    if (status == CDC_U2_OK) {
        status = cdc_u2_monodromy_append(&order_trace, &order_b,
                                         CDC_U2_EVENT_FLOW, 1.0, -1);
    }
    if (status != CDC_U2_OK ||
        memcmp(order_trace.matrix.data, order_expected.data,
               4 * sizeof(double)) != 0) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    memset(&recurrence_spec, 0, sizeof(recurrence_spec));
    recurrence_spec.kind = CDC_U2_RECURRENCE_RELATIVE;
    recurrence_spec.tolerance = 1e-12;
    recurrence_spec.restore_endpoint = self_test_restore;
    recurrence_spec.restoration_context = &shift;
    recurrence_spec.restoration_equivariant = 1;
    cdc_matrix_identity(&restoration_derivative);
    recurrence_spec.restoration_derivative = &restoration_derivative;
    recurrence_spec.discrete_state_verified = 1;
    status = cdc_u2_recurrence_check(&layout, initial, final, 6,
                                     &recurrence_spec, &recurrence);
    if (status != CDC_U2_OK || recurrence.authorizes_monodromy) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    status = cdc_u2_monodromy_bind_recurrence(
        &monodromy, &recurrence_spec, &recurrence, 1.0);
    if (status != CDC_U2_OK || !recurrence.authorizes_monodromy ||
        !recurrence.restoration_derivative_applied ||
        monodromy.event_count != 3 ||
        monodromy.events[2].kind != CDC_U2_EVENT_ENDPOINT_RESTORATION) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    /* A drift in one packed coordinate is not recurrence.  This also keeps
     * the ACCEPT_FALSE_RECURRENCE test mutant permanently killable. */
    final[2] += 1.0;
    memset(&recurrence_spec, 0, sizeof(recurrence_spec));
    recurrence_spec.kind = CDC_U2_RECURRENCE_FULL;
    recurrence_spec.tolerance = 1e-12;
    recurrence_spec.discrete_state_verified = 1;
    status = cdc_u2_recurrence_check(&layout, initial, final, 6,
                                     &recurrence_spec, &recurrence);
    if (status != CDC_U2_NOT_RECURRENT || recurrence.authorizes_monodromy) {
        status = CDC_U2_VALIDATION_FAILED;
        goto cleanup;
    }
    status = CDC_U2_OK;

cleanup:
    cdc_u2_monodromy_release(&monodromy);
    cdc_u2_monodromy_release(&order_trace);
    cdc_matrix_release(&local);
    cdc_matrix_release(&nest);
    cdc_matrix_release(&reset);
    cdc_matrix_release(&saltation);
    cdc_matrix_release(&order_a);
    cdc_matrix_release(&order_b);
    cdc_matrix_release(&order_expected);
    cdc_matrix_release(&restoration_derivative);
    cdc_u2_layout_release(&layout);
    return status;
}
