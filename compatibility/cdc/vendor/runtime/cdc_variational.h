#ifndef CDC_VARIATIONAL_H
#define CDC_VARIATIONAL_H

#include "cdc_linalg.h"

#include <stddef.h>

#define CDC_U2_COORDINATE_NAME_MAX 128

typedef enum {
    CDC_U2_OK = 0,
    CDC_U2_INVALID_ARGUMENT,
    CDC_U2_DIMENSION_MISMATCH,
    CDC_U2_ALLOCATION_FAILED,
    CDC_U2_NONFINITE,
    CDC_U2_MODE_DIVERGENCE,
    CDC_U2_GUARD_NOT_LOCALIZED,
    CDC_U2_NONTRANSVERSE_EVENT,
    CDC_U2_NOT_RECURRENT,
    CDC_U2_RELATIVE_RESTORATION_UNVERIFIED,
    CDC_U2_NEUTRAL_MODE_UNVERIFIED,
    CDC_U2_SPECTRAL_BACKEND_UNAVAILABLE,
    CDC_U2_SPECTRAL_BACKEND_FAILED,
    CDC_U2_VALIDATION_FAILED
} cdc_u2_status;

const char *cdc_u2_status_reason(cdc_u2_status status);

typedef enum {
    CDC_U2_COORDINATE_THETA,
    CDC_U2_COORDINATE_BELIEF,
    CDC_U2_COORDINATE_PRIOR
} cdc_u2_coordinate_kind;

typedef struct {
    cdc_u2_coordinate_kind kind;
    size_t source_index;
    double period; /* 2*pi for theta, zero for an ordinary real coordinate */
    char name[CDC_U2_COORDINATE_NAME_MAX];
} cdc_u2_coordinate;

typedef struct {
    size_t dimension;
    size_t cell_count;
    size_t module_count;
    cdc_u2_coordinate *coordinates;
} cdc_u2_layout;

/* Deterministic executable-state order:
 *   every cell theta in source declaration order,
 *   then belief and prior for every module in source declaration order.
 */
cdc_u2_status cdc_u2_layout_build(
    cdc_u2_layout *layout,
    const char *const *cell_names, size_t cell_count,
    const char *const *module_names, size_t module_count);
void cdc_u2_layout_release(cdc_u2_layout *layout);
long cdc_u2_layout_find(const cdc_u2_layout *layout, const char *name);
/* Override coordinate topology. period=0 means unwrapped R; period>0 means
 * shortest-period distance. Native stability uses this for lifted cover cells. */
cdc_u2_status cdc_u2_layout_set_period(cdc_u2_layout *layout,
                                       size_t coordinate, double period);

cdc_u2_status cdc_u2_state_pack(
    const cdc_u2_layout *layout,
    const double *theta, size_t theta_count,
    const double *belief, const double *prior, size_t module_count,
    double *packed, size_t packed_count);
cdc_u2_status cdc_u2_state_unpack(
    const cdc_u2_layout *layout,
    const double *packed, size_t packed_count,
    double *theta, size_t theta_count,
    double *belief, double *prior, size_t module_count);

typedef struct {
    size_t source_theta;
    size_t target_theta;
    double coefficient; /* field gain * channel weight * flow duration */
    double angle;
} cdc_u2_flow_coupling;

/* Exact Jacobian of the executable explicit-Euler phase update.  Frequencies
 * contribute an affine drift and therefore leave the identity derivative.
 * Couplings must already be filtered to the flow's field by the caller. */
cdc_u2_status cdc_u2_flow_jacobian(
    const cdc_u2_layout *layout,
    const double *state, size_t state_count,
    const cdc_u2_flow_coupling *couplings, size_t coupling_count,
    cdc_matrix *jacobian);

/* Exact derivative of executable nest in one fixed latch/trit mode:
 *   parent.belief' = parent.belief + gain * constant_mean_trit
 *   child.prior'   = parent.belief'
 * child_belief and gain are included to make the executable/theoretical
 * distinction testable; the production derivative does not use them. */
cdc_u2_status cdc_u2_nest_jacobian(
    size_t dimension,
    size_t parent_belief,
    size_t child_belief,
    size_t child_prior,
    double gain,
    cdc_matrix *jacobian);

typedef enum {
    CDC_U2_COMMIT_SCHEDULED,
    CDC_U2_COMMIT_GUARD_TRIGGERED
} cdc_u2_commit_kind;

/* Current CDC commit is scheduled and mutates only discrete latches.  Its
 * fixed-mode continuous derivative is identity.  Guard-triggered requests
 * hold until the primal runtime localizes the event; no saltation is faked. */
cdc_u2_status cdc_u2_commit_jacobian(
    size_t dimension,
    cdc_u2_commit_kind kind,
    int fixed_mode,
    cdc_matrix *jacobian);

/* Autonomous/non-autonomous hybrid saltation, available to a future primal
 * event executor once it supplies a real localized crossing:
 * S = DR + ((f+ - DR f- - R_t) n^T)/(n^T f- + g_t).
 * reset_time_derivative may be NULL to mean zero. */
cdc_u2_status cdc_u2_saltation_matrix(
    const cdc_matrix *reset_jacobian,
    const double *flow_before,
    const double *flow_after,
    const double *guard_normal,
    const double *reset_time_derivative,
    size_t dimension,
    double guard_time_derivative,
    double transversality_tolerance,
    cdc_matrix *saltation,
    double *denominator_out);

typedef enum {
    CDC_U2_EVENT_FLOW,
    CDC_U2_EVENT_SCHEDULED_COMMIT,
    CDC_U2_EVENT_GUARD_TRANSITION,
    CDC_U2_EVENT_NEST,
    CDC_U2_EVENT_ENDPOINT_RESTORATION
} cdc_u2_event_kind;

const char *cdc_u2_event_kind_name(cdc_u2_event_kind kind);

typedef struct {
    cdc_u2_event_kind kind;
    double time;
    int transverse; /* -1 not applicable, 0 false, 1 true */
} cdc_u2_event;

typedef struct {
    size_t dimension;
    cdc_matrix matrix;
    cdc_u2_event *events;
    size_t event_count;
    size_t event_capacity;
} cdc_u2_monodromy;

cdc_u2_status cdc_u2_monodromy_init(cdc_u2_monodromy *monodromy,
                                    size_t dimension);
void cdc_u2_monodromy_release(cdc_u2_monodromy *monodromy);

/* Local maps are appended in execution order and composed as
 * M <- local * M. */
cdc_u2_status cdc_u2_monodromy_append(
    cdc_u2_monodromy *monodromy,
    const cdc_matrix *local,
    cdc_u2_event_kind kind,
    double time,
    int transverse);

typedef enum {
    CDC_U2_RECURRENCE_FULL,
    CDC_U2_RECURRENCE_RELATIVE
} cdc_u2_recurrence_kind;

const char *cdc_u2_recurrence_kind_name(cdc_u2_recurrence_kind kind);

typedef cdc_u2_status (*cdc_u2_endpoint_restoration_fn)(
    const double *final_state,
    size_t dimension,
    double *restored_state,
    void *context);

typedef struct {
    cdc_u2_recurrence_kind kind;
    /* `tolerance` is the source-v1 compatibility spelling for an absolute
     * tolerance. New callers should set absolute_tolerance explicitly. */
    double tolerance;
    double absolute_tolerance;
    double relative_tolerance;
    /* Optional positive finite per-coordinate weights. */
    const double *weights;
    /* Optional dimension-byte mask. NULL includes every coordinate.  Any
     * exclusion makes the result a projected diagnostic that cannot
     * authorize monodromy or Floquet classification. */
    const unsigned char *include;
    /* Relative recurrence is admitted only through an executable endpoint
     * restoration whose equivariance has been witnessed by the caller.
     * The derivative of this same restoration must be appended to M. */
    cdc_u2_endpoint_restoration_fn restore_endpoint;
    void *restoration_context;
    int restoration_equivariant;
    const cdc_matrix *restoration_derivative;
    /* Latches/modes are outside the packed continuous vector.  The primal
     * executor must prove that they recur (after restoration, if any). */
    int discrete_state_verified;
} cdc_u2_recurrence_spec;

typedef struct {
    cdc_u2_recurrence_kind kind;
    double residual;
    double tolerance;
    double absolute_tolerance;
    double relative_tolerance;
    double normalized_residual;
    double maximum_allowed_residual;
    size_t included_coordinates;
    int projected;
    int restoration_applied;
    int restoration_equivariant;
    int restoration_derivative_bound;
    int restoration_derivative_applied;
    int discrete_state_verified;
    int verified;
    int authorizes_monodromy;
} cdc_u2_recurrence_result;

/* Residual is deterministic L-infinity distance. Theta coordinates use their
 * declared periodic topology. A mask is useful for diagnosis but is never a
 * proof that omitted drift recurs. */
cdc_u2_status cdc_u2_recurrence_check(
    const cdc_u2_layout *layout,
    const double *initial_state,
    const double *final_state,
    size_t state_count,
    const cdc_u2_recurrence_spec *spec,
    cdc_u2_recurrence_result *result);

/* Finalizes the return tangent. Full recurrence binds without changing M.
 * Relative recurrence left-multiplies the executable path derivative by the
 * declared endpoint-restoration derivative, M_rel = D(rho) * D(U). Spectrum
 * refuses a relative result until this operation succeeds. */
cdc_u2_status cdc_u2_monodromy_bind_recurrence(
    cdc_u2_monodromy *monodromy,
    const cdc_u2_recurrence_spec *spec,
    cdc_u2_recurrence_result *result,
    double endpoint_time);

typedef enum {
    CDC_U2_MULTIPLIER_PHYSICAL,
    CDC_U2_MULTIPLIER_GAUGE
} cdc_u2_multiplier_mode;

typedef struct {
    double real;
    double imag;
    double modulus;
    cdc_u2_multiplier_mode mode;
} cdc_u2_multiplier;

typedef enum {
    CDC_U2_STABILITY_STABLE,
    CDC_U2_STABILITY_MARGINAL,
    CDC_U2_STABILITY_UNSTABLE
} cdc_u2_stability_class;

const char *cdc_u2_stability_class_name(cdc_u2_stability_class classification);

typedef struct {
    size_t dimension;
    cdc_u2_multiplier *multipliers;
    cdc_u2_stability_class classification;
    double spectral_radius;
    double schur_reconstruction_residual;
    double schur_orthogonality_residual;
    double schur_triangular_residual;
    char backend[32];
} cdc_u2_spectrum;

void cdc_u2_spectrum_release(cdc_u2_spectrum *spectrum);

/* A spectrum requires a recurrence result that authorizes monodromy. Optional
 * gauge_generators is an n-by-k matrix of explicit tangent generators.  A
 * generator is labelled gauge only after non-degeneracy/independence checks
 * and M*g ~= g validation; proximity of an eigenvalue to +1 is insufficient. */
cdc_u2_status cdc_u2_spectrum_compute(
    const cdc_matrix *monodromy,
    const cdc_real_schur_backend *backend,
    const cdc_u2_recurrence_result *recurrence,
    const cdc_matrix *gauge_generators,
    double neutral_tolerance,
    double schur_validation_tolerance,
    cdc_u2_spectrum *spectrum);

typedef cdc_u2_status (*cdc_u2_map_fn)(const double *input, size_t dimension,
                                       double *output, void *context);

/* A polarity witness is a derived, scoped covariance test. It does not add a
 * reduction and it does not infer a symmetry from receptive/radiant names.
 * The caller supplies the original and conjugate executable maps, the typed
 * polarity involutions on their source/target spaces, and all four exact
 * derivatives. A symmetry-breaking realization remains valid CDC; this
 * witness simply returns accepted=0 with a typed reason. */
typedef enum {
    CDC_U2_POLARITY_NONE = 0,
    CDC_U2_POLARITY_MAP_NOT_INVOLUTIVE,
    CDC_U2_POLARITY_APERTURE_NOT_FIXED,
    CDC_U2_POLARITY_CLOSURE_NOT_COVARIANT,
    CDC_U2_POLARITY_TANGENT_NOT_COVARIANT
} cdc_u2_polarity_reason;

const char *cdc_u2_polarity_reason_name(cdc_u2_polarity_reason reason);

typedef struct {
    size_t dimension;
    double tolerance;
    const double *source_state;
    const double *aperture_state;
    cdc_u2_map_fn source_polarity;
    void *source_polarity_context;
    cdc_u2_map_fn target_polarity;
    void *target_polarity_context;
    cdc_u2_map_fn primal_map;
    void *primal_context;
    cdc_u2_map_fn conjugate_primal_map;
    void *conjugate_primal_context;
    const cdc_matrix *source_polarity_derivative;
    const cdc_matrix *target_polarity_derivative;
    const cdc_matrix *primal_derivative;
    const cdc_matrix *conjugate_primal_derivative;
} cdc_u2_polarity_spec;

typedef struct {
    int accepted;
    cdc_u2_polarity_reason reason;
    double source_involution_residual;
    double target_involution_residual;
    double aperture_fixed_residual;
    double closure_covariance_residual;
    double tangent_covariance_residual;
} cdc_u2_polarity_result;

/* Checks rho_1 U = U^rho rho_0 and its tangent consequence
 * D(rho_1) D(U) = D(U^rho) D(rho_0), in addition to involution and a fixed
 * aperture. Residuals are scale-normalized L-infinity distances. */
cdc_u2_status cdc_u2_polarity_covariance_check(
    const cdc_u2_polarity_spec *spec,
    cdc_u2_polarity_result *result);

/* Adaptive-scale central-difference oracle for verification only. */
cdc_u2_status cdc_u2_validate_jacobian(
    cdc_u2_map_fn map,
    void *context,
    const double *point,
    size_t dimension,
    const cdc_matrix *analytic,
    double relative_step,
    double absolute_tolerance,
    double *max_error_out);

/* Internal mathematical smoke gate.  It covers exact executable nest
 * semantics, Euler flow differentiation, composition order, recurrence, and
 * a closed-form saltation case.  It does not require a spectral backend. */
cdc_u2_status cdc_u2_self_test(void);

#endif
