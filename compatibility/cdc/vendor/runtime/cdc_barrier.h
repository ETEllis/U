#ifndef CDC_BARRIER_H
#define CDC_BARRIER_H

#include <stddef.h>

/*
 * The balanced-ternary commit barrier shared by in-memory latching,
 * persistence, and RFTC contract probes.
 *
 * A vector is admissible when its running balance never falls below zero.
 * The final balance may be nonzero: the calculus protects prefix viability,
 * not only closed walks. Invalid carrier symbols fail closed.
 */
typedef struct {
    int admissible;
    int final_balance;
    size_t trit_count;
    size_t first_violation;
} cdc_barrier_result;

static int cdc_barrier_trit_value(char trit, int *value_out) {
    if (!value_out) {
        return 0;
    }
    if (trit == '+') {
        *value_out = 1;
    } else if (trit == '0') {
        *value_out = 0;
    } else if (trit == '-') {
        *value_out = -1;
    } else {
        return 0;
    }
    return 1;
}

static int cdc_barrier_evaluate(const char *trits,
                                cdc_barrier_result *result) {
    size_t i;
    int balance = 0;

    if (!trits || !result || trits[0] == '\0') {
        return 0;
    }
    result->admissible = 1;
    result->final_balance = 0;
    result->trit_count = 0;
    result->first_violation = (size_t)-1;
    for (i = 0; trits[i] != '\0'; i++) {
        int value;
        if (!cdc_barrier_trit_value(trits[i], &value)) {
            return 0;
        }
        balance += value;
        if (balance < 0 && result->admissible) {
            result->admissible = 0;
            result->first_violation = i;
        }
    }
    result->final_balance = balance;
    result->trit_count = i;
    return 1;
}

#endif
