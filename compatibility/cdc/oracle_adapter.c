/* Differential-test adapter only. U execution never links this translation unit.
 * Compile with the pinned BiDi runtime include path. The original source remains
 * the oracle; hex-float output avoids the CLI's six-decimal presentation loss. */
#define main cdc_original_main
#include "cdc_native_runtime.c"
#undef main

int main(int argc, char **argv) {
    Runtime *rt;
    if (argc != 2) return 2;
    rt = (Runtime *)calloc(1, sizeof(Runtime));
    if (!rt) return 3;
    parse_source(rt, argv[1]);
    printf("{\"steps\":[");
    for (int i = 0; i < rt->step_count; i++) {
        Step *step = &rt->steps[i];
        if (i) printf(",");
        if (step->kind == STEP_FLOW) {
            FlowResult r;
            execute_flow(rt, step, &r);
            printf("{\"kind\":\"flow\"}");
        } else if (step->kind == STEP_COMMIT) {
            CommitResult r;
            execute_commit(rt, step, &r);
            printf("{\"kind\":\"commit\",\"status\":\"%s\",\"trits\":\"%s\"}", r.status, r.trits);
        } else {
            NestResult r;
            execute_nest(rt, step, &r);
            printf("{\"kind\":\"nest\"}");
        }
    }
    printf("],\"cells\":[");
    for (int i = 0; i < rt->cell_count; i++) {
        Cell *c = &rt->cells[i];
        printf("%s{\"theta\":\"%a\",\"has_latch\":%s,\"latch\":%d}",
               i ? "," : "", c->theta, c->has_latch ? "true" : "false", trit_value(c->latch));
    }
    printf("],\"modules\":[");
    for (int i = 0; i < rt->module_count; i++) {
        Module *m = &rt->modules[i];
        printf("%s{\"belief\":\"%a\",\"prior\":\"%a\"}", i ? "," : "", m->belief, m->prior);
    }
    printf("]}\n");
    free(rt);
    return 0;
}
