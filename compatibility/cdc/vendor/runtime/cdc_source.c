#include "cdc_source.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cdc_source_fail(const char *message) {
    fprintf(stderr, "cdc-source: %s\n", message);
    exit(1);
}

int cdc_close_enough(double actual, double expected, double tolerance) {
    return fabs(actual - expected) <= tolerance;
}

void cdc_expect_string(const char *actual, const char *expected, const char *message) {
    if (strcmp(actual, expected) != 0) {
        cdc_source_fail(message);
    }
}

void cdc_expect_int(int actual, int expected, const char *message) {
    if (actual != expected) {
        cdc_source_fail(message);
    }
}

void cdc_expect_double(double actual, double expected, double tolerance, const char *message) {
    if (!cdc_close_enough(actual, expected, tolerance)) {
        cdc_source_fail(message);
    }
}
