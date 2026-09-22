#ifndef U_NATIVE_RUNTIME_H
#define U_NATIVE_RUNTIME_H
#include <stddef.h>
typedef struct UValue *V;
typedef struct UEnv *E;
typedef V (*UFn)(E, size_t, V *);
V u_int(const char *decimal);
V u_real(const char *decimal);
V u_text(const char *utf8);
V u_text_n(const char *utf8, size_t bytes);
V u_bool(int truth);
V u_unit(void);
V u_list(size_t count, V *items);
V u_tuple(size_t count, V *items);
V u_record(size_t count, const char **keys, V *items);
V u_closure(UFn fn, E captured, size_t arity);
V u_annotate(V closure, V descriptor);
V u_descriptor(const char *json, size_t bytes);
V u_literal(const char *utf8, size_t bytes);
V u_guard(V value, const char *storage_kind, int nonnegative);
E u_env(E parent);
void u_bind(E env, const char *name, V value);
void u_bind_lazy(E env, const char *name, V initializer);
V u_lookup(E env, const char *name);
V u_member(V object, const char *name);
V u_call(V function, size_t count, V *arguments);
void u_runtime_init(int argc, char **argv);
V u_entry(E root, const char *name);
int u_finish(V result);
#endif
