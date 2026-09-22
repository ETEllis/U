#define _POSIX_C_SOURCE 200809L
#include "runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <float.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include <setjmp.h>
#include <dirent.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif
_Static_assert(sizeof(double)==8 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024,"U native binary64 requires IEEE-754 double");
_Static_assert(ATOMIC_INT_LOCK_FREE==2 && ATOMIC_LLONG_LOCK_FREE==2,"isolated worker status and budgets require lock-free shared atomics");

/* This file contains the disclosed value/call/platform bridge, not a source
 * evaluator. U functions are ordinary compiled functions behind UFn pointers. */
enum UKind { K_UNIT,K_BOOL,K_INT,K_REAL,K_TEXT,K_LIST,K_TUPLE,K_RECORD,
             K_CLOSURE,K_NAMESPACE,K_CELL,K_BUFFER,K_PARTIAL,K_AGAIN,K_RESOURCE,K_TABLE,K_PROCESS,K_FUTURE };
typedef struct { _Atomic int result_ready,failed,cancelled; } ProcessShared;
typedef struct { pid_t pid,owner;int fd,result_fd,exited,status;ProcessShared *shared;V outcome; } Process;
typedef struct { int sign; size_t n; uint32_t *d; } Big;
typedef struct { size_t n,cap; V *v; } Seq;
typedef struct { const char *key;size_t length;uint64_t hash;V value; } Slot;
struct UValue {
    enum UKind kind;
    union {
        int boolean; double real; Big integer;
        struct { size_t n; char *s; } text;
        Seq seq;
        struct { size_t n; char **keys; V *values; } record;
        struct { UFn fn; E env; size_t arity; const char *primitive; V descriptor; } closure;
        struct { const char *kind; V payload; int live; pid_t owner_pid; } resource;
        struct { size_t n,cap;Slot *slots; } table;
        Process process;
        Process *future;
        V inner;
    } as;
};
struct Binding { const char *name; V value; struct Binding *next,*hash_next; int lazy_state; };
struct ForceFrame { struct Binding *binding;struct ForceFrame *previous; };
struct Temporary { void *pointer;size_t size;struct Temporary *next;void (*release)(void*); };
struct ErrorFrame { jmp_buf jump;struct ErrorFrame *previous;size_t depth;V pending;struct ForceFrame *forcing;struct Temporary *temporary;char code[96],message[4096]; };
struct EnvIndex { struct Binding **buckets;size_t count,capacity; };
struct UEnv { E parent;struct Binding *bindings;struct EnvIndex *index; };
struct Allocation { struct Allocation *next;size_t used,capacity;max_align_t alignment; };
static struct Allocation *allocations;
static struct Allocation *arena_page;
static size_t allocated, allocation_limit = (size_t)1024*1024*1024;
static size_t call_depth;
static int runtime_argc;
static char **runtime_argv;
static V pending_continuation;
struct ProcessRecord { V handle;struct ProcessRecord *next; };
static struct ProcessRecord *process_records;
static size_t active_processes;
static ProcessShared *current_worker_shared;
static int current_worker_completion_fd=-1;
static _Atomic uint64_t *shared_call_budget;
static struct ErrorFrame *error_frame;
static struct ForceFrame *forcing_bindings;
static struct Temporary *temporaries;
static size_t temporary_bytes;
static void terminate_owned_processes(void);
static void temporary_cleanup(struct Temporary *checkpoint);
static struct UValue unit_value = { .kind=K_UNIT };
static struct UValue true_value = { .kind=K_BOOL,.as.boolean=1 };
static struct UValue false_value = { .kind=K_BOOL,.as.boolean=0 };
static struct UValue small_integers[4098];
static uint32_t small_limbs[4098];
static int small_initialized;

static void fail(const char *code,const char *message) {
    int terminal=!strcmp(code,"BUDGET_EXHAUSTED")||!strcmp(code,"MEMORY_LIMIT")||!strcmp(code,"ALLOCATION_OVERFLOW")||!strcmp(code,"ALLOCATION")||!strcmp(code,"NATIVE_RESULT")||!strcmp(code,"CONTINUATION_CONTEXT");
    if(error_frame&&!terminal&&!(shared_call_budget&&atomic_load(shared_call_budget+1))){
        snprintf(error_frame->code,sizeof(error_frame->code),"%s",code);size_t n=strlen(message);if(n>=sizeof(error_frame->message)){n=sizeof(error_frame->message)-1;while(n&&((unsigned char)message[n]&0xc0)==0x80)n--;}memcpy(error_frame->message,message,n);error_frame->message[n]=0;
        while(forcing_bindings!=error_frame->forcing){forcing_bindings->binding->lazy_state=1;forcing_bindings=forcing_bindings->previous;}
        temporary_cleanup(error_frame->temporary);
        call_depth=error_frame->depth;pending_continuation=error_frame->pending;longjmp(error_frame->jump,1);
    }
    if(current_worker_shared)atomic_store(&current_worker_shared->failed,1);
    terminate_owned_processes();
    fprintf(stderr,"U_NATIVE_ERROR %s: %s\n",code,message); exit(70);
}
static size_t product(size_t n,size_t size) {
    if (size && n > SIZE_MAX/size) fail("ALLOCATION_OVERFLOW","allocation size overflow");
    return n*size;
}
static void *allocate(size_t size) {
    if (!size) size=1;
    if (size>allocation_limit || allocated>allocation_limit-size || size>SIZE_MAX-sizeof(struct Allocation))
        fail("MEMORY_LIMIT","native arena exhausted; U_NATIVE_MEMORY_MB controls the disclosed process budget");
    size_t alignment=_Alignof(max_align_t);if(size>SIZE_MAX-(alignment-1))fail("ALLOCATION_OVERFLOW","aligned allocation size overflow");size_t padded=(size+alignment-1)&~(alignment-1);
    struct Allocation *p=padded<=65536?arena_page:NULL;
    if(!p||padded>p->capacity-p->used){
        size_t capacity=padded>65536?padded:65536;if(capacity>SIZE_MAX-sizeof(*p))fail("ALLOCATION_OVERFLOW","arena page size overflow");p=calloc(1,sizeof(*p)+capacity);if(!p)fail("ALLOCATION","operating system refused arena page allocation");
        p->capacity=capacity;p->next=allocations;allocations=p;if(padded<=65536)arena_page=p;
    }
    void *value=(unsigned char*)(p+1)+p->used;p->used+=padded;allocated+=size;return value;
}
static void *array(size_t n,size_t size) { return allocate(product(n,size)); }
static void *temporary_malloc(size_t size){
    if(!size)size=1;if(size>allocation_limit||temporary_bytes>allocation_limit-size)fail("MEMORY_LIMIT","temporary native storage budget exceeded");
    void *pointer=malloc(size);struct Temporary *record=malloc(sizeof(*record));if(!pointer||!record)fail("ALLOCATION","temporary native storage allocation failed");
    record->pointer=pointer;record->size=size;record->next=temporaries;record->release=free;temporaries=record;temporary_bytes+=size;return pointer;
}
static void temporary_resource(void *pointer,void (*release)(void*)){
    struct Temporary *record=malloc(sizeof(*record));if(!record)fail("ALLOCATION","temporary resource tracking allocation failed");record->pointer=pointer;record->size=0;record->release=release;record->next=temporaries;temporaries=record;
}
static void temporary_forget(void *pointer){
    struct Temporary **at=&temporaries;while(*at&&(*at)->pointer!=pointer)at=&(*at)->next;if(!*at)fail("NATIVE_RESULT","untracked temporary resource");struct Temporary *record=*at;*at=record->next;temporary_bytes-=record->size;free(record);
}
static void release_file(void *file){fclose(file);}
static void release_directory(void *directory){closedir(directory);}
#ifndef __APPLE__
static void release_digest(void *context){EVP_MD_CTX_free(context);}
#endif
static FILE *open_output(const char *path,int exclusive){
    FILE *file;if(exclusive){int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0666);if(fd<0)fail("IO_WRITE",strerror(errno));file=fdopen(fd,"wb");if(!file){close(fd);fail("IO_WRITE",strerror(errno));}}else file=fopen(path,"wb");if(!file)fail("IO_WRITE",strerror(errno));temporary_resource(file,release_file);return file;
}
static void temporary_free(void *pointer){
    if(!pointer)return;struct Temporary **at=&temporaries;while(*at&&(*at)->pointer!=pointer)at=&(*at)->next;if(!*at)fail("NATIVE_RESULT","untracked temporary native storage");
    struct Temporary *record=*at;*at=record->next;temporary_bytes-=record->size;record->release(record->pointer);free(record);
}
static void *temporary_realloc(void *pointer,size_t size){
    if(!pointer)return temporary_malloc(size);struct Temporary *record=temporaries;while(record&&record->pointer!=pointer)record=record->next;if(!record)fail("NATIVE_RESULT","untracked temporary resize");
    if(size>allocation_limit||temporary_bytes-record->size>allocation_limit-size)fail("MEMORY_LIMIT","temporary native storage budget exceeded");void *next=realloc(pointer,size);if(!next)fail("ALLOCATION","temporary native storage resize failed");temporary_bytes=temporary_bytes-record->size+size;record->pointer=next;record->size=size;return next;
}
static void temporary_cleanup(struct Temporary *checkpoint){
    while(temporaries!=checkpoint){if(!temporaries)fail("NATIVE_RESULT","temporary storage checkpoint was invalidated");struct Temporary *record=temporaries;temporaries=record->next;temporary_bytes-=record->size;record->release(record->pointer);free(record);}
}
static char *copy_bytes(const char *s,size_t n) {
    if (n==SIZE_MAX) fail("ALLOCATION_OVERFLOW","text length overflow");
    char *r=allocate(n+1); memcpy(r,s,n); return r;
}
static V fresh(enum UKind kind) { V v=allocate(sizeof(*v)); v->kind=kind; return v; }
static void expected(V v,enum UKind kind,const char *name) {
    if (!v || v->kind!=kind) fail("TYPE",name);
}
static void arity(size_t got,size_t want) { if (got!=want) fail("ARITY","argument count does not match operation"); }
static int boolean(V v) { expected(v,K_BOOL,"expected Bool"); return v->as.boolean; }
static const char *cstring(V v) {
    expected(v,K_TEXT,"expected Text");
    if (strlen(v->as.text.s)!=v->as.text.n) fail("NUL_PATH","platform argument contains embedded NUL");
    return v->as.text.s;
}
static uint32_t scalar(const char *s,size_t n,size_t *i) {
    if (*i>=n) fail("UTF8","unexpected end of UTF-8");
    unsigned char a=(unsigned char)s[(*i)++];
    if (a<128) return a;
    int count; uint32_t cp,min;
    if (a>=0xc2 && a<=0xdf) {count=1;cp=a&31;min=0x80;}
    else if (a>=0xe0 && a<=0xef) {count=2;cp=a&15;min=0x800;}
    else if (a>=0xf0 && a<=0xf4) {count=3;cp=a&7;min=0x10000;}
    else {fail("UTF8","invalid UTF-8 leading byte");return 0;}
    while (count--) {
        if (*i>=n || ((unsigned char)s[*i]&0xc0)!=0x80) fail("UTF8","invalid UTF-8 continuation");
        cp=(cp<<6)|((unsigned char)s[(*i)++]&63);
    }
    if (cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) fail("UTF8","invalid Unicode scalar");
    return cp;
}
V u_text_n(const char *s,size_t n) {
    size_t i=0; while(i<n) scalar(s,n,&i);
    V v=fresh(K_TEXT);v->as.text.n=n;v->as.text.s=copy_bytes(s,n);return v;
}
V u_text(const char *s) { return u_text_n(s,strlen(s)); }
V u_unit(void) { return &unit_value; }
V u_bool(int b) { return b?&true_value:&false_value; }
static Big big_new(size_t n) { Big b={0,n,array(n,sizeof(uint32_t))};return b; }
static Big normalize(Big b) { while(b.n && !b.d[b.n-1]) --b.n; if(!b.n)b.sign=0;return b; }
static V cached_small(int64_t n){
    if(n< -1||n>4096)return NULL;
    if(!small_initialized){for(int i=0;i<4098;i++){int value=i-1;small_limbs[i]=(uint32_t)(value<0?-value:value);small_integers[i].kind=K_INT;small_integers[i].as.integer=(Big){value<0?-1:value>0?1:0,value?1:0,&small_limbs[i]};}small_initialized=1;}
    return &small_integers[n+1];
}
static V big_value(Big b) { b=normalize(b);if(b.n<=1){V cached=cached_small(b.n?(int64_t)b.d[0]*b.sign:0);if(cached)return cached;}V v=fresh(K_INT);v->as.integer=b;return v; }
#define BASE UINT32_C(1000000000)
V u_int(const char *s) {
    int sign=1;if(*s=='-'){sign=-1;++s;}else if(*s=='+')++s;
    size_t len=strlen(s);if(!len)fail("INTEGER","empty integer literal");
    for(size_t i=0;i<len;i++)if(s[i]<'0'||s[i]>'9')fail("INTEGER","invalid integer decimal");
    if(len<=4){int64_t n=0;for(size_t i=0;i<len;i++)n=n*10+s[i]-'0';V cached=cached_small(n*sign);if(cached)return cached;}
    Big b=big_new((len+8)/9);b.sign=sign;
    for(size_t i=0,end=len;end;i++) {
        size_t start=end>9?end-9:0;uint32_t d=0;
        for(size_t j=start;j<end;j++)d=d*10+(uint32_t)(s[j]-'0');
        b.d[i]=d;end=start;
    }
    return big_value(b);
}
static V size_value(size_t n) {if(n<=4096)return cached_small((int64_t)n);char s[3*sizeof(size_t)+2];snprintf(s,sizeof(s),"%zu",n);return u_int(s); }
static V signed_value(int64_t n){V cached=cached_small(n);if(cached)return cached;uint64_t mag=n<0?(uint64_t)(-(n+1))+1:(uint64_t)n;Big b=big_new(mag>=BASE?mag>=(uint64_t)BASE*BASE?3:2:1);b.sign=n<0?-1:n>0?1:0;for(size_t i=0;i<b.n;i++){b.d[i]=(uint32_t)(mag%BASE);mag/=BASE;}return big_value(b);}
static Big integer(V v) {expected(v,K_INT,"expected Int");return v->as.integer;}
static size_t index_value(V v) {
    Big b=integer(v);if(b.sign<0)fail("INDEX","expected nonnegative integer");
    size_t n=0;
    for(size_t i=b.n;i--;) {if(n>(SIZE_MAX-b.d[i])/BASE)fail("INDEX","integer exceeds addressable index");n=n*BASE+b.d[i];}
    return n;
}
static int magnitude(Big a,Big b) {
    if(a.n!=b.n)return a.n>b.n?1:-1;
    for(size_t i=a.n;i--;)if(a.d[i]!=b.d[i])return a.d[i]>b.d[i]?1:-1;
    return 0;
}
static int compare(Big a,Big b) {
    if(a.sign!=b.sign)return a.sign>b.sign?1:-1;
    return a.sign*magnitude(a,b);
}
static Big mag_add(Big a,Big b) {
    size_t n=a.n>b.n?a.n:b.n;Big r=big_new(n+1);uint64_t carry=0;r.sign=1;
    for(size_t i=0;i<n;i++){uint64_t s=carry+(i<a.n?a.d[i]:0)+(i<b.n?b.d[i]:0);r.d[i]=s%BASE;carry=s/BASE;}
    r.d[n]=(uint32_t)carry;return normalize(r);
}
static Big mag_sub(Big a,Big b) {
    Big r=big_new(a.n);r.sign=1;int64_t carry=0;
    for(size_t i=0;i<a.n;i++){int64_t d=(int64_t)a.d[i]-(i<b.n?b.d[i]:0)-carry;carry=d<0;if(carry)d+=BASE;r.d[i]=(uint32_t)d;}
    return normalize(r);
}
static Big big_add(Big a,Big b) {
    if(!a.sign)return b;if(!b.sign)return a;
    if(a.sign==b.sign){Big r=mag_add(a,b);r.sign=a.sign;return r;}
    int cmp=magnitude(a,b);if(!cmp)return big_new(0);
    Big r=cmp>0?mag_sub(a,b):mag_sub(b,a);r.sign=cmp>0?a.sign:b.sign;return r;
}
static Big big_mul(Big a,Big b) {
    if(!a.sign||!b.sign)return big_new(0);
    if(a.n>SIZE_MAX-b.n)fail("ALLOCATION_OVERFLOW","integer multiplication length");
    Big r=big_new(a.n+b.n);r.sign=a.sign*b.sign;
    for(size_t i=0;i<a.n;i++){
        uint64_t carry=0;
        for(size_t j=0;j<b.n;j++){uint64_t v=(uint64_t)a.d[i]*b.d[j]+r.d[i+j]+carry;r.d[i+j]=v%BASE;carry=v/BASE;}
        r.d[i+b.n]=(uint32_t)carry;
    }
    return normalize(r);
}
/* Schoolbook long division: each radix digit is selected by binary search.
 * Scratch storage is reused, so digit search does not grow the arena. */
static Big big_div(Big a,Big b,Big *rem) {
    if(!b.sign)fail("DIVISION_ZERO","integer division by zero");
    Big q=big_new(a.n),r=big_new(b.n+1),trial=big_new(b.n+1);q.sign=a.sign*b.sign;r.n=0;r.sign=0;
    for(size_t pos=a.n;pos--;){
        if(r.n)memmove(r.d+1,r.d,r.n*sizeof(uint32_t));r.d[0]=a.d[pos];r.n++;r.sign=1;r=normalize(r);
        uint32_t lo=0,hi=BASE-1,best=0;
        while(lo<=hi){
            uint32_t m=lo+(hi-lo)/2;uint64_t carry=0;
            for(size_t j=0;j<b.n;j++){uint64_t t=(uint64_t)b.d[j]*m+carry;trial.d[j]=t%BASE;carry=t/BASE;}
            trial.d[b.n]=(uint32_t)carry;trial.n=b.n+1;trial.sign=1;trial=normalize(trial);
            if(magnitude(trial,r)<=0){best=m;if(m==BASE-1)break;lo=m+1;}else{if(!m)break;hi=m-1;}
        }
        uint64_t carry=0;int64_t borrow=0;
        for(size_t j=0;j<r.n;j++){
            uint64_t t=(j<b.n?(uint64_t)b.d[j]*best:0)+carry;carry=t/BASE;
            int64_t d=(int64_t)r.d[j]-(int64_t)(t%BASE)-borrow;borrow=d<0;if(borrow)d+=BASE;r.d[j]=(uint32_t)d;
        }
        r=normalize(r);q.d[pos]=best;
    }
    r.sign=r.n?a.sign:0;if(rem)*rem=r;return normalize(q);
}
static char *decimal(Big b) {
    if(!b.n)return copy_bytes("0",1);
    size_t cap=product(b.n,9)+3;char *s=allocate(cap);size_t at=0;if(b.sign<0)s[at++]='-';
    at+=(size_t)snprintf(s+at,cap-at,"%u",b.d[b.n-1]);
    for(size_t i=b.n-1;i--;)at+=(size_t)snprintf(s+at,cap-at,"%09u",b.d[i]);return s;
}
static V real_value(double x) { V v=fresh(K_REAL);v->as.real=x;return v; }
V u_real(const char *s) {
    errno=0;char *end;double x=strtod(s,&end);
    if(!*s||*end||!isfinite(x))fail("REAL","invalid finite binary64 literal");
    return real_value(x);
}
static double real(V v) {expected(v,K_REAL,"expected binary64 Real");return v->as.real;}
V u_guard(V value,const char *kind,int nonnegative){
    static const char *names[]={"unit","bool","int","real","text","list","tuple","record","closure","namespace","cell","buffer","partial","continuation","resource","table","process","future"};
    if(!strcmp(kind,"numeric")){if(value->kind!=K_INT&&value->kind!=K_REAL)fail("TYPE","expected numeric representation");}
    else if(strcmp(names[value->kind],kind))fail("TYPE",kind);
    if(nonnegative){expected(value,K_INT,"nonnegative constraint requires Int");if(value->as.integer.sign<0)fail("NATURAL","negative natural argument");}return value;
}
static V sequence(enum UKind kind,size_t n,V *items) {
    V v=fresh(kind);v->as.seq.n=v->as.seq.cap=n;v->as.seq.v=array(n,sizeof(V));
    for(size_t i=0;i<n;i++){if(items[i]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");v->as.seq.v[i]=items[i];}return v;
}
V u_list(size_t n,V *items) {return sequence(K_LIST,n,items);}
V u_tuple(size_t n,V *items) {return sequence(K_TUPLE,n,items);}
V u_record(size_t n,const char **keys,V *items) {
    V v=fresh(K_RECORD);v->as.record.n=n;v->as.record.keys=array(n,sizeof(char*));v->as.record.values=array(n,sizeof(V));
    for(size_t i=0;i<n;i++){
        for(size_t j=0;j<i;j++)if(!strcmp(keys[i],keys[j]))fail("DUPLICATE_FIELD","duplicate record key");
        if(items[i]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");
        v->as.record.keys[i]=copy_bytes(keys[i],strlen(keys[i]));v->as.record.values[i]=items[i];
    }return v;
}
V u_closure(UFn fn,E env,size_t n) {V v=fresh(K_CLOSURE);v->as.closure.fn=fn;v->as.closure.env=env;v->as.closure.arity=n;return v;}
V u_annotate(V closure,V descriptor) {expected(closure,K_CLOSURE,"only closures accept compiler metadata");if(descriptor->kind!=K_TEXT&&descriptor->kind!=K_RECORD)fail("DESCRIPTOR","closure descriptor must be Text or Record");closure->as.closure.descriptor=descriptor;return closure;}
struct Descriptor { const char *address;size_t length;V text;struct Descriptor *next; };
static struct Descriptor *descriptors[4096];
V u_descriptor(const char *json,size_t bytes){
    size_t bucket=(((uintptr_t)json>>3)^((uintptr_t)json>>15))&4095;
    for(struct Descriptor *d=descriptors[bucket];d;d=d->next)if(d->address==json&&d->length==bytes)return d->text;
    struct Descriptor *d=allocate(sizeof(*d));d->address=json;d->length=bytes;d->text=u_text_n(json,bytes);d->next=descriptors[bucket];descriptors[bucket]=d;return d->text;
}
V u_literal(const char *utf8,size_t bytes){return u_descriptor(utf8,bytes);}
E u_env(E parent) {E e=allocate(sizeof(*e));e->parent=parent;return e;}
static uint64_t name_hash(const char *name){uint64_t hash=UINT64_C(14695981039346656037);while(*name){hash^=(unsigned char)*name++;hash*=UINT64_C(1099511628211);}return hash;}
static struct Binding *env_binding(E env,const char *name,uint64_t hash){
    if(env->index){for(struct Binding *b=env->index->buckets[(size_t)hash&(env->index->capacity-1)];b;b=b->hash_next)if(!strcmp(b->name,name))return b;}
    else for(struct Binding *b=env->bindings;b;b=b->next)if(!strcmp(b->name,name))return b;
    return NULL;
}
static void env_grow(E env,size_t count){
    if(!env->index)env->index=allocate(sizeof(*env->index));struct EnvIndex *index=env->index;
    size_t cap=index->capacity?index->capacity*2:64;if(cap<index->capacity)fail("ALLOCATION_OVERFLOW","environment index capacity");
    index->buckets=array(cap,sizeof(struct Binding*));index->capacity=cap;index->count=count;
    for(struct Binding *b=env->bindings;b;b=b->next){size_t at=(size_t)name_hash(b->name)&(cap-1);b->hash_next=index->buckets[at];index->buckets[at]=b;}
}
void u_bind(E env,const char *name,V value) {
    if(value->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation must be returned directly");
    uint64_t hash=name_hash(name);if(env_binding(env,name,hash))fail("DUPLICATE_BINDING","duplicate binding in one scope");
    struct Binding *b=allocate(sizeof(*b));b->name=name;b->value=value;b->next=env->bindings;env->bindings=b;
    size_t count;if(env->index)count=++env->index->count;else{count=0;for(struct Binding *item=env->bindings;item;item=item->next)count++;}
    if((!env->index&&count>=16)||(env->index&&count>env->index->capacity/2))env_grow(env,count);
    else if(env->index){size_t at=(size_t)hash&(env->index->capacity-1);b->hash_next=env->index->buckets[at];env->index->buckets[at]=b;}
}
void u_bind_lazy(E env,const char *name,V initializer){expected(initializer,K_CLOSURE,"lazy initializer must be a closure");arity(initializer->as.closure.arity,0);u_bind(env,name,initializer);env->bindings->lazy_state=1;}

typedef struct { const char *name; size_t arity; } Primitive;
static const Primitive primitives[]={
 {"int.add",2},{"int.sub",2},{"int.mul",2},{"int.div",2},{"int.mod",2},
 {"int.eq",2},{"int.ne",2},{"int.lt",2},{"int.le",2},{"int.gt",2},{"int.ge",2},
 {"int.neg",1},{"int.abs",1},{"int.to_real",1},
 {"nat.zero",0},{"nat.succ",1},{"nat.add",2},{"nat.sub",2},{"nat.mul",2},{"nat.div",2},{"nat.mod",2},
 {"nat.eq",2},{"nat.lt",2},{"nat.le",2},{"nat.gt",2},{"nat.ge",2},{"nat.ceil_div",2},{"nat.rec",3},
 {"bool.not",1},{"bool.and",2},{"bool.or",2},
 {"text.concat",2},{"text.eq",2},{"text.length",1},{"text.at",2},{"text.slice",3},
 {"text.from_code",1},{"text.code",1},{"text.join",2},{"text.parse_int",1},{"text.parse_real",1},
 {"text.from_int",1},{"text.from_real",1},{"bytes.length",1},{"bytes.at",2},{"bytes.slice",3},
 {"bytes.find_any",3},{"bytes.find",3},
 {"bytes.from_hex",1},
 {"list.length",1},{"list.get",2},{"list.cons",2},{"list.head_or",2},{"list.tail_or_empty",1},
 {"list.reverse",1},{"list.append",2},{"list.map",2},{"list.filter",2},{"list.fold_left",3},
 {"tuple.first",1},{"tuple.second",1},{"tuple.get",2},{"tuple.make",1},{"tuple.length",1},
 {"record.get",2},{"record.has",2},{"record.keys",1},{"record.make",2},{"record.put",3},
 {"cell.new",1},{"cell.get",1},{"cell.set",2},
 {"buffer.new",0},{"buffer.push",2},{"buffer.get",2},{"buffer.set",3},{"buffer.length",1},{"buffer.freeze",1},
 {"value.select",3},{"value.kind",1},{"value.equal",2},{"value.to_text",1},{"value.fail",1},
 {"value.unit",0},{"value.describe",1},{"value.arity",1},{"value.call",2},
 {"value.try_call",2},
 {"resource.new",2},{"resource.peek",2},{"resource.take",2},{"resource.kind",1},
 {"table.new",0},{"table.set",3},{"table.get",3},{"table.has",2},
 {"control.while",2},{"core.fix_partial",2},{"partial.done",1},{"partial.value",1},
 {"sys.args",0},{"sys.read",1},{"sys.write",2},{"sys.print",1},{"sys.exit",1},{"sys.exec",1},{"sys.getenv",1},{"sys.native_names",0},
 {"sys.has_cap",1},{"capability.require",1},
 {"sys.stdin_line",0},{"sys.stdin_read",1},{"sys.stdout_write",1},
 {"sys.write_bytes",2},{"sys.write_bytes_new",2},{"sys.write_new",2},{"sys.list_dir",1},{"sys.file_kind",1},{"sys.mkdir",1},{"sys.rename",2},{"sys.cwd",0},
 {"process.spawn",1},{"process.poll",1},{"process.wait",2},{"process.cancel",1},{"process.sleep",1},
 {"process.share",1},{"process.result",2},{"process.observe",1},
 {"process.spawn_with_caps",2},{"http.transport",1},
 {"crypto.sha256",1},{"crypto.sha256_file",1},
 {"json.parse",1},{"json.encode",1},
 {"f64.add",2},{"f64.sub",2},{"f64.mul",2},{"f64.div",2},{"f64.eq",2},{"f64.lt",2},{"f64.le",2},{"f64.gt",2},{"f64.ge",2},
 {"f64.bits",1},
 {"f32.add",2},{"f32.sub",2},{"f32.mul",2},{"f32.div",2},{"f32.from_real",1},
 {"math.sin",1},{"math.cos",1},{"math.exp",1},{"math.log",1},{"math.sqrt",1},{"math.abs",1},{"math.fma",3},{"math.is_finite",1},
 {"random.uniform",2},{"sum.make",2},{"sum.case",2},{"option.when",2}
};
#define PRIMITIVE_COUNT (sizeof(primitives)/sizeof(primitives[0]))
static V primitive_cache[PRIMITIVE_COUNT];
static V namespace_cache[PRIMITIVE_COUNT+1];
typedef struct { const char *name;const Primitive *primitive; } PrimitiveEntry;
#define PRIMITIVE_BUCKETS 2048
_Static_assert(PRIMITIVE_COUNT*4<PRIMITIVE_BUCKETS,"primitive registry hash table requires spare capacity");
static PrimitiveEntry primitive_index[PRIMITIVE_BUCKETS];
static int primitive_index_ready;
static char *native_alias(const char *name) {
    size_t n=strlen(name);char *alias=allocate(n+8);memcpy(alias,"native.",7);
    for(size_t i=0;i<n;i++)alias[i+7]=name[i]=='.'?'_':name[i];return alias;
}
static const Primitive *primitive_named(const char *name) {
    if(!primitive_index_ready){
        for(size_t i=0;i<PRIMITIVE_COUNT;i++){const char *names[]={primitives[i].name,native_alias(primitives[i].name)};for(size_t j=0;j<2;j++){size_t at=(size_t)name_hash(names[j])&(PRIMITIVE_BUCKETS-1);while(primitive_index[at].name)at=(at+1)&(PRIMITIVE_BUCKETS-1);primitive_index[at]=(PrimitiveEntry){names[j],&primitives[i]};}}
        primitive_index_ready=1;
    }
    size_t at=(size_t)name_hash(name)&(PRIMITIVE_BUCKETS-1);
    while(primitive_index[at].name){if(!strcmp(primitive_index[at].name,name))return primitive_index[at].primitive;at=(at+1)&(PRIMITIVE_BUCKETS-1);}return NULL;
}
static V primitive_value(const Primitive *p) {size_t i=(size_t)(p-primitives);if(primitive_cache[i])return primitive_cache[i];V v=u_closure(NULL,NULL,p->arity);v->as.closure.primitive=p->name;primitive_cache[i]=v;return v; }
static int namespace_exists(const char *name) {
    if(!strcmp(name,"native"))return 1;
    size_t n=strlen(name);
    for(size_t i=0;i<PRIMITIVE_COUNT;i++)if(!strncmp(primitives[i].name,name,n)&&primitives[i].name[n]=='.')return 1;
    return 0;
}
V u_lookup(E env,const char *name) {
    uint64_t hash=name_hash(name);
    for(E e=env;e;e=e->parent){struct Binding *b=env_binding(e,name,hash);if(b){
        if(b->lazy_state==2)fail("INITIALIZATION_CYCLE","cyclic lazy definition initialization");
        if(b->lazy_state==1){struct ForceFrame force={b,forcing_bindings};forcing_bindings=&force;b->lazy_state=2;b->value=u_call(b->value,0,NULL);b->lazy_state=0;forcing_bindings=force.previous;}return b->value;
    }}
    const Primitive *p=primitive_named(name);if(p)return primitive_value(p);
    if(namespace_exists(name)){size_t slot=0;while(namespace_cache[slot]){if(!strcmp(namespace_cache[slot]->as.text.s,name))return namespace_cache[slot];slot++;}V v=fresh(K_NAMESPACE);v->as.text.s=copy_bytes(name,strlen(name));v->as.text.n=strlen(name);namespace_cache[slot]=v;return v;}
    fail("UNBOUND_NAME",name);return NULL;
}
static long field(V object,const char *name) {
    expected(object,K_RECORD,"expected Record");
    for(size_t i=0;i<object->as.record.n;i++)if(!strcmp(object->as.record.keys[i],name))return (long)i;
    return -1;
}
V u_member(V object,const char *name) {
    if(object->kind==K_TUPLE){size_t i;if(!strcmp(name,"first"))i=0;else if(!strcmp(name,"second"))i=1;else{fail("MISSING_FIELD",name);return NULL;}if(i>=object->as.seq.n)fail("INDEX","tuple member projection out of bounds");return object->as.seq.v[i];}
    if(object->kind==K_NAMESPACE){
        size_t a=object->as.text.n,b=strlen(name);if(a>SIZE_MAX-b-2)fail("ALLOCATION_OVERFLOW","qualified name length");
        char local[256];char *qualified=a+b+2<=sizeof(local)?local:temporary_malloc(a+b+2);memcpy(qualified,object->as.text.s,a);qualified[a]='.';memcpy(qualified+a+1,name,b+1);
        V result=u_lookup(NULL,qualified);if(qualified!=local)temporary_free(qualified);return result;
    }
    long i=field(object,name);if(i<0)fail("MISSING_FIELD",name);return object->as.record.values[i];
}
static int equal(V a,V b,size_t depth) {
    if(depth>512)fail("VALUE_DEPTH","structural equality depth exceeded");
    if(a->kind==K_AGAIN||b->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation is not ordinary data");
    if(a==b)return 1;if(a->kind!=b->kind)return 0;
    switch(a->kind){
    case K_UNIT:return 1;case K_BOOL:return a->as.boolean==b->as.boolean;
    case K_INT:return !compare(a->as.integer,b->as.integer);
    case K_REAL:return a->as.real==b->as.real;
    case K_TEXT:return a->as.text.n==b->as.text.n&&!memcmp(a->as.text.s,b->as.text.s,a->as.text.n);
    case K_LIST:case K_TUPLE:
        if(a->as.seq.n!=b->as.seq.n)return 0;
        for(size_t i=0;i<a->as.seq.n;i++)if(!equal(a->as.seq.v[i],b->as.seq.v[i],depth+1))return 0;return 1;
    case K_RECORD:
        if(a->as.record.n!=b->as.record.n)return 0;
        for(size_t i=0;i<a->as.record.n;i++){long j=field(b,a->as.record.keys[i]);if(j<0||!equal(a->as.record.values[i],b->as.record.values[j],depth+1))return 0;}return 1;
    case K_PARTIAL:return equal(a->as.inner,b->as.inner,depth+1);
    default:return 0;
    }
}
typedef struct { char *s; size_t n,cap; } TextBuffer;
static void push_bytes(TextBuffer *b,const char *s,size_t n) {
    if(n>allocation_limit||b->n>allocation_limit-n-1)fail("MEMORY_LIMIT","text builder budget exceeded");
    size_t need=b->n+n+1;
    if(need>b->cap){size_t cap=b->cap?b->cap:128;while(cap<need){if(cap>allocation_limit/2){cap=need;break;}cap*=2;}char *next=temporary_realloc(b->s,cap);b->s=next;b->cap=cap;}
    memcpy(b->s+b->n,s,n);b->n+=n;b->s[b->n]=0;
}
static void push(TextBuffer *b,const char *s) {push_bytes(b,s,strlen(s));}
static void json_string(TextBuffer *b,const char *s,size_t n) {
    push(b,"\"");
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];char escape[7];
        if(c=='"')push(b,"\\\"");else if(c=='\\')push(b,"\\\\");
        else if(c<32){snprintf(escape,sizeof(escape),"\\u%04x",c);push(b,escape);}else push_bytes(b,(const char*)&s[i],1);
    }push(b,"\"");
}
static void encode(TextBuffer *b,V v,size_t depth) {
    if(depth>256)fail("JSON_DEPTH","JSON nesting exceeds 256");
    switch(v->kind){
    case K_UNIT:push(b,"null");break;
    case K_BOOL:push(b,v->as.boolean?"true":"false");break;
    case K_INT:push(b,decimal(v->as.integer));break;
    case K_REAL:{char s[64];if(!isfinite(v->as.real))fail("JSON_REAL","non-finite values are not JSON numbers");snprintf(s,sizeof(s),"%.17g",v->as.real);push(b,s);if(!strchr(s,'.')&&!strchr(s,'e')&&!strchr(s,'E'))push(b,".0");break;}
    case K_TEXT:json_string(b,v->as.text.s,v->as.text.n);break;
    case K_LIST:case K_TUPLE:
        push(b,"[");for(size_t i=0;i<v->as.seq.n;i++){if(i)push(b,",");encode(b,v->as.seq.v[i],depth+1);}push(b,"]");break;
    case K_RECORD:
        push(b,"{");for(size_t i=0;i<v->as.record.n;i++){if(i)push(b,",");json_string(b,v->as.record.keys[i],strlen(v->as.record.keys[i]));push(b,":");encode(b,v->as.record.values[i],depth+1);}push(b,"}");break;
    case K_PARTIAL:push(b,"{\"status\":\"done\",\"value\":");encode(b,v->as.inner,depth+1);push(b,"}");break;
    default:fail("JSON_VALUE","value has no JSON data encoding");
    }
}
static V json_encode(V v) {TextBuffer b={0};encode(&b,v,0);V result=u_text_n(b.s,b.n);temporary_free(b.s);return result;}
typedef struct { const char *s;size_t n,i; } JsonInput;
static void ws(JsonInput *j){while(j->i<j->n&&(j->s[j->i]==' '||j->s[j->i]=='\n'||j->s[j->i]=='\r'||j->s[j->i]=='\t'))j->i++;}
static uint32_t hex4(JsonInput *j){
    uint32_t n=0;for(int i=0;i<4;i++){if(j->i>=j->n)fail("JSON","incomplete Unicode escape");char c=j->s[j->i++];int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(d<0)fail("JSON","invalid Unicode escape");n=n*16+(unsigned)d;}return n;
}
static size_t utf8(uint32_t cp,char *s){
    if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))fail("UNICODE_SCALAR","invalid Unicode scalar");
    if(cp<0x80){s[0]=(char)cp;return 1;}if(cp<0x800){s[0]=(char)(0xc0|(cp>>6));s[1]=(char)(0x80|(cp&63));return 2;}
    if(cp<0x10000){s[0]=(char)(0xe0|(cp>>12));s[1]=(char)(0x80|((cp>>6)&63));s[2]=(char)(0x80|(cp&63));return 3;}
    s[0]=(char)(0xf0|(cp>>18));s[1]=(char)(0x80|((cp>>12)&63));s[2]=(char)(0x80|((cp>>6)&63));s[3]=(char)(0x80|(cp&63));return 4;
}
static V json_read_string(JsonInput *j){
    if(j->i>=j->n||j->s[j->i++]!='"')fail("JSON","expected string");TextBuffer b={0};int closed=0;
    while(j->i<j->n){char c=j->s[j->i++];if(c=='"'){closed=1;break;}if((unsigned char)c<32)fail("JSON","raw string control byte");
        if(c=='\\'){
            if(j->i>=j->n)fail("JSON","unterminated escape");c=j->s[j->i++];
            switch(c){case '"':case '\\':case '/':break;case 'b':c='\b';break;case 'f':c='\f';break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;
            case 'u':{uint32_t cp=hex4(j);if(cp>=0xd800&&cp<=0xdbff){if(j->i+2>j->n||j->s[j->i]!='\\'||j->s[j->i+1]!='u')fail("JSON","unpaired high surrogate");j->i+=2;uint32_t lo=hex4(j);if(lo<0xdc00||lo>0xdfff)fail("JSON","invalid surrogate pair");cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);}char s[4];size_t n=utf8(cp,s);push_bytes(&b,s,n);continue;}
            default:fail("JSON","unknown string escape");}
        }push_bytes(&b,&c,1);
    }
    if(!closed)fail("JSON","unterminated string");V v=u_text_n(b.s?b.s:"",b.n);temporary_free(b.s);return v;
}
static V json_read(JsonInput *j,size_t depth){
    if(depth>256)fail("JSON_DEPTH","JSON nesting exceeds 256");ws(j);if(j->i>=j->n)fail("JSON","expected value");char c=j->s[j->i];
    if(c=='"')return json_read_string(j);
    if(c=='['||c=='{'){
        j->i++;int object=c=='{';char end=object?'}':']';size_t n=0,cap=8;V *items=temporary_malloc(cap*sizeof(V));const char **keys=object?temporary_malloc(cap*sizeof(char*)):NULL;ws(j);
        if(j->i<j->n&&j->s[j->i]==end){j->i++;V v=object?u_record(0,NULL,NULL):u_list(0,NULL);temporary_free(items);temporary_free(keys);return v;}
        for(;;){
            if(n==cap){if(cap>allocation_limit/(2*sizeof(V)))fail("MEMORY_LIMIT","JSON collection budget");cap*=2;items=temporary_realloc(items,cap*sizeof(V));if(object)keys=temporary_realloc(keys,cap*sizeof(char*));}
            ws(j);if(object){keys[n]=cstring(json_read_string(j));ws(j);if(j->i>=j->n||j->s[j->i++]!=':')fail("JSON","expected object colon");}
            items[n++]=json_read(j,depth+1);ws(j);if(j->i>=j->n)fail("JSON","unterminated collection");char next=j->s[j->i++];if(next==end)break;if(next!=',')fail("JSON","expected collection comma");
        }
        V v=object?u_record(n,keys,items):u_list(n,items);temporary_free(items);temporary_free(keys);return v;
    }
    if(j->n-j->i>=4&&!memcmp(j->s+j->i,"true",4)){j->i+=4;return u_bool(1);}
    if(j->n-j->i>=5&&!memcmp(j->s+j->i,"false",5)){j->i+=5;return u_bool(0);}
    if(j->n-j->i>=4&&!memcmp(j->s+j->i,"null",4)){j->i+=4;return u_unit();}
    size_t start=j->i;int floating=0;if(c=='-')j->i++;
    if(j->i>=j->n||j->s[j->i]<'0'||j->s[j->i]>'9')fail("JSON","expected number");
    if(j->s[j->i]=='0')j->i++;else while(j->i<j->n&&j->s[j->i]>='0'&&j->s[j->i]<='9')j->i++;
    if(j->i<j->n&&j->s[j->i]=='.'){floating=1;j->i++;size_t before=j->i;while(j->i<j->n&&j->s[j->i]>='0'&&j->s[j->i]<='9')j->i++;if(before==j->i)fail("JSON","fraction requires digits");}
    if(j->i<j->n&&(j->s[j->i]=='e'||j->s[j->i]=='E')){floating=1;j->i++;if(j->i<j->n&&(j->s[j->i]=='+'||j->s[j->i]=='-'))j->i++;size_t before=j->i;while(j->i<j->n&&j->s[j->i]>='0'&&j->s[j->i]<='9')j->i++;if(before==j->i)fail("JSON","exponent requires digits");}
    char *text=copy_bytes(j->s+start,j->i-start);return floating?u_real(text):u_int(text);
}
static V json_parse(V text){expected(text,K_TEXT,"json.parse expects Text");JsonInput j={text->as.text.s,text->as.text.n,0};V v=json_read(&j,0);ws(&j);if(j.i!=j.n)fail("JSON","trailing JSON input");return v;}
static V invoke_primitive(const char *name,size_t n,V *a);
V u_call(V function,size_t n,V *a) {
    if(shared_call_budget){uint64_t remaining=atomic_load(shared_call_budget);for(;;){if(!remaining){atomic_store(shared_call_budget+1,1);fail("BUDGET_EXHAUSTED","native call-dispatch allowance exhausted");}if(atomic_compare_exchange_weak(shared_call_budget,&remaining,remaining-1))break;}}
    expected(function,K_CLOSURE,"expected callable closure");arity(n,function->as.closure.arity);
    if(++call_depth>2048)fail("CALL_DEPTH","native call nesting exceeds 2048; use explicit tail iteration");
    V result=function->as.closure.primitive?invoke_primitive(function->as.closure.primitive,n,a):function->as.closure.fn(function->as.closure.env,n,a);
    --call_depth;if(!result)fail("NATIVE_RESULT","native function returned no value");
    if(shared_call_budget&&atomic_load(shared_call_budget+1))fail("BUDGET_EXHAUSTED","shared worker call-dispatch allowance exhausted");
    if(pending_continuation&&result!=pending_continuation)fail("CONTINUATION_CONTEXT","non-tail continuation context would be discarded");return result;
}
static V call0(V f){return u_call(f,0,NULL);}
static V call1(V f,V x){V a[]={x};return u_call(f,1,a);}
static V call2(V f,V x,V y){V a[]={x,y};return u_call(f,2,a);}
static Seq list(V v){expected(v,K_LIST,"expected List");return v->as.seq;}
static size_t checked_index(V v,size_t n){size_t i=index_value(v);if(i>=n)fail("INDEX","collection index out of bounds");return i;}
static size_t text_offset(V v,size_t index,int allow_end){
    expected(v,K_TEXT,"expected Text");size_t i=0,k=0;while(k<index&&i<v->as.text.n){scalar(v->as.text.s,v->as.text.n,&i);k++;}
    if(k!=index||(!allow_end&&i==v->as.text.n))fail("INDEX","text scalar index out of bounds");return i;
}
static V again_call(E env,size_t n,V *a){(void)env;arity(n,1);if(pending_continuation)fail("CONTINUATION_CONTEXT","nested unresolved continuation");V v=fresh(K_AGAIN);v->as.inner=a[0];pending_continuation=v;return v;}
static void reserve_buffer(V v,size_t n){
    expected(v,K_BUFFER,"expected Buffer");if(n<=v->as.seq.cap)return;size_t cap=v->as.seq.cap?v->as.seq.cap:8;
    while(cap<n){if(cap>SIZE_MAX/2)fail("ALLOCATION_OVERFLOW","buffer length overflow");cap*=2;}
    V *items=array(cap,sizeof(V));if(v->as.seq.n)memcpy(items,v->as.seq.v,v->as.seq.n*sizeof(V));v->as.seq.v=items;v->as.seq.cap=cap;
}
static void buffer_push(V v,V item){if(item->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");if(v->as.seq.n==SIZE_MAX)fail("ALLOCATION_OVERFLOW","buffer length overflow");reserve_buffer(v,v->as.seq.n+1);v->as.seq.v[v->as.seq.n++]=item;}
static int capability_granted(const char *wanted){
    const char *allow=getenv("U_NATIVE_ALLOW");size_t n=strlen(wanted);
    if(allow)while(*allow){while(*allow==','||*allow==' ')allow++;const char *end=allow;while(*end&&*end!=','&&*end!=' ')end++;if((size_t)(end-allow)==n&&!memcmp(allow,wanted,n))return 1;allow=end;}return 0;
}
static void capability(const char *wanted){if(!capability_granted(wanted))fail("CAPABILITY",wanted);}
static const char *file_kind(const char *path){struct stat info;if(lstat(path,&info)){if(errno==ENOENT||errno==ENOTDIR)return "missing";fail("FILE_STAT",strerror(errno));}return S_ISREG(info.st_mode)?"file":S_ISDIR(info.st_mode)?"directory":S_ISLNK(info.st_mode)?"symlink":"other";}
static V transport_pack(V value,size_t depth){
    if(depth>128)fail("TRANSPORT_DEPTH","worker data exceeds nesting limit");V payload=value;int tag=(int)value->kind;
    switch(value->kind){
    case K_UNIT:case K_BOOL:case K_TEXT:break;
    case K_INT:payload=u_text(decimal(value->as.integer));break;
    case K_REAL:{char number[80];if(!isfinite(value->as.real))fail("TRANSPORT_REAL","worker data requires finite real values");snprintf(number,sizeof(number),"%a",value->as.real);payload=u_text(number);break;}
    case K_LIST:case K_TUPLE:{Seq xs=value->as.seq;V *items=array(xs.n,sizeof(V));for(size_t i=0;i<xs.n;i++)items[i]=transport_pack(xs.v[i],depth+1);payload=u_list(xs.n,items);break;}
    case K_RECORD:{size_t n=value->as.record.n;V *items=array(n,sizeof(V));for(size_t i=0;i<n;i++)items[i]=transport_pack(value->as.record.values[i],depth+1);payload=u_record(n,(const char**)value->as.record.keys,items);break;}
    case K_PARTIAL:payload=transport_pack(value->as.inner,depth+1);break;
    default:fail("TRANSPORT_CARRIER","worker transport accepts immutable ordinary data, not resources, cells or closures");
    }
    V pair[]={size_value((size_t)tag),payload};return u_list(2,pair);
}
static V transport_unpack(V packed,size_t depth){
    if(depth>128)fail("TRANSPORT_DEPTH","worker data exceeds nesting limit");Seq pair=list(packed);if(pair.n!=2)fail("TRANSPORT_FORMAT","invalid tagged worker data");size_t tag=index_value(pair.v[0]);V p=pair.v[1];
    switch(tag){
    case K_UNIT:expected(p,K_UNIT,"invalid transported Unit");return p;
    case K_BOOL:expected(p,K_BOOL,"invalid transported Bool");return p;
    case K_TEXT:expected(p,K_TEXT,"invalid transported Text");return p;
    case K_INT:return u_int(cstring(p));case K_REAL:return u_real(cstring(p));
    case K_LIST:case K_TUPLE:{Seq xs=list(p);V *items=array(xs.n,sizeof(V));for(size_t i=0;i<xs.n;i++)items[i]=transport_unpack(xs.v[i],depth+1);return tag==K_LIST?u_list(xs.n,items):u_tuple(xs.n,items);}
    case K_RECORD:{expected(p,K_RECORD,"invalid transported Record");size_t n=p->as.record.n;V *items=array(n,sizeof(V));for(size_t i=0;i<n;i++)items[i]=transport_unpack(p->as.record.values[i],depth+1);return u_record(n,(const char**)p->as.record.keys,items);}
    case K_PARTIAL:{V v=fresh(K_PARTIAL);v->as.inner=transport_unpack(p,depth+1);return v;}
    default:fail("TRANSPORT_FORMAT","unrecognized worker data tag");return NULL;
    }
}
static V process_outcome(const char *state,V value,const char *error,int cancellation){
    const char *keys[]={"state","value","error","cancellation_requested"};V values[]={u_text(state),value,error?u_text(error):u_unit(),u_bool(cancellation)};return u_record(4,keys,values);
}
static void process_reap(Process *p){
    if(!p->exited&&p->owner==getpid()){
        pid_t got=waitpid(p->pid,&p->status,WNOHANG);
        if(got==p->pid){p->exited=1;active_processes--;}
        else if(got<0&&errno!=EINTR)fail("PROCESS_WAIT",strerror(errno));
    }
}
static V process_observe(Process *p){
    process_reap(p);
    if(p->outcome)return p->outcome;
    /* A completion pipe has no payload. EOF is persistent and wakes every
     * subscriber even if a worker dies before writing a result. Subscribers
     * read the immutable result file using pread, so none consume another's data. */
    struct pollfd ready={p->fd,POLLIN|POLLHUP,0};if(poll(&ready,1,0)<0&&errno!=EINTR)fail("PROCESS_POLL",strerror(errno));
    int finished=(ready.revents&(POLLHUP|POLLIN))!=0;
    if(p->exited)finished=1;
    int cancelled=atomic_load(&p->shared->cancelled);
    if(!finished)return process_outcome("running",u_unit(),NULL,cancelled);
    if(atomic_load(&p->shared->result_ready)&&!atomic_load(&p->shared->failed)){
        TextBuffer text={0};char chunk[8192];off_t offset=0;
        for(;;){ssize_t n=pread(p->result_fd,chunk,sizeof(chunk),offset);if(n<0&&errno==EINTR)continue;if(n<0)fail("PROCESS_READ",strerror(errno));if(!n)break;if(text.n+(size_t)n>64*1024*1024)fail("TRANSPORT_LIMIT","worker result exceeds 64 MiB");push_bytes(&text,chunk,(size_t)n);offset+=n;}
        if(!text.n)fail("TRANSPORT_EMPTY","worker returned no data");V data=transport_unpack(json_parse(u_text_n(text.s,text.n)),0);temporary_free(text.s);p->outcome=process_outcome("completed",data,NULL,cancelled);
    }else p->outcome=process_outcome(cancelled?"cancelled":"failed",u_unit(),cancelled?"isolated worker cancellation requested; effects are not rolled back":"isolated worker terminated without a completed result",cancelled);
    close(p->fd);p->fd=-1;close(p->result_fd);p->result_fd=-1;munmap(p->shared,sizeof(*p->shared));p->shared=NULL;
    return p->outcome;
}
static Process *owned_process(V handle){expected(handle,K_PROCESS,"expected isolated worker owner");Process *p=&handle->as.process;if(p->owner!=getpid())fail("PROCESS_OWNER","only the creating process may wait, cancel or issue reading rights");return p;}
static V process_poll(V handle){return process_observe(owned_process(handle));}
static void terminate_owned_processes(void){
    struct ProcessRecord *records=process_records;process_records=NULL;
    for(struct ProcessRecord *record=records;record;record=record->next){Process *p=&record->handle->as.process;if(!p->exited){kill(p->pid,SIGTERM);while(waitpid(p->pid,&p->status,0)<0&&errno==EINTR){}}if(p->fd>=0)close(p->fd);if(p->result_fd>=0)close(p->result_fd);if(p->shared)munmap(p->shared,sizeof(*p->shared));}
    active_processes=0;
}
static double monotonic_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))fail("CLOCK",strerror(errno));return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1000000.0;}
static uint64_t text_hash(V key){expected(key,K_TEXT,"table key must be Text");uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<key->as.text.n;i++){h^=(unsigned char)key->as.text.s[i];h*=UINT64_C(1099511628211);}return h;}
static Slot *table_slot(V table,V key,uint64_t hash){
    size_t mask=table->as.table.cap-1,index=(size_t)hash&mask;
    for(;;){Slot *slot=&table->as.table.slots[index];if(!slot->key||(slot->hash==hash&&slot->length==key->as.text.n&&!memcmp(slot->key,key->as.text.s,slot->length)))return slot;index=(index+1)&mask;}
}
static void table_grow(V table){
    size_t oldcap=table->as.table.cap;Slot *old=table->as.table.slots;if(oldcap>SIZE_MAX/2)fail("ALLOCATION_OVERFLOW","table size overflow");
    table->as.table.cap=oldcap?oldcap*2:64;table->as.table.slots=array(table->as.table.cap,sizeof(Slot));
    for(size_t i=0;i<oldcap;i++)if(old[i].key){size_t index=(size_t)old[i].hash&(table->as.table.cap-1);while(table->as.table.slots[index].key)index=(index+1)&(table->as.table.cap-1);table->as.table.slots[index]=old[i];}
}
static V invoke_primitive(const char *name,size_t n,V *a){
    (void)n;
    if(!strncmp(name,"int.",4)||!strncmp(name,"nat.",4)){
        int natural=name[0]=='n';const char *op=name+4;
        if(!strcmp(op,"zero"))return u_int("0");
        if(!strcmp(op,"rec")){size_t count=index_value(a[0]);V state=a[1];for(size_t i=0;i<count;i++)state=call2(a[2],size_value(i),state);return state;}
        Big x=integer(a[0]);if(natural&&x.sign<0)fail("NATURAL","negative natural input");
        if(!strcmp(op,"neg")){x.sign=-x.sign;return big_value(x);}if(!strcmp(op,"abs")){if(x.sign<0)x.sign=1;return big_value(x);}
        if(!strcmp(op,"to_real")){double d=strtod(decimal(x),NULL);if(!isfinite(d))fail("REAL_RANGE","integer outside finite binary64 range");return real_value(d);}
        Big y=!strcmp(op,"succ")?integer(u_int("1")):integer(a[1]);if(natural&&y.sign<0)fail("NATURAL","negative natural input");int cmp=compare(x,y);
        if(!strcmp(op,"eq"))return u_bool(!cmp);if(!strcmp(op,"ne"))return u_bool(cmp!=0);if(!strcmp(op,"lt"))return u_bool(cmp<0);if(!strcmp(op,"le"))return u_bool(cmp<=0);if(!strcmp(op,"gt"))return u_bool(cmp>0);if(!strcmp(op,"ge"))return u_bool(cmp>=0);
        if(x.n<=1&&y.n<=1){
            int64_t lx=x.n?(int64_t)x.d[0]*x.sign:0,ly=y.n?(int64_t)y.d[0]*y.sign:0,result=0;int handled=1;
            if(!strcmp(op,"add")||!strcmp(op,"succ"))result=lx+ly;
            else if(!strcmp(op,"sub"))result=lx-ly;
            else if(!strcmp(op,"mul"))result=lx*ly;
            else if(!strcmp(op,"div")||!strcmp(op,"mod")||!strcmp(op,"ceil_div")){if(!ly)fail("DIVISION_ZERO","integer division by zero");result=!strcmp(op,"mod")?lx%ly:!strcmp(op,"ceil_div")?(lx+ly-1)/ly:lx/ly;}
            else handled=0;
            if(handled){if(natural&&result<0)fail("NATURAL","natural subtraction underflow");return signed_value(result);}
        }
        Big r;if(!strcmp(op,"add")||!strcmp(op,"succ"))r=big_add(x,y);
        else if(!strcmp(op,"sub")){y.sign=-y.sign;r=big_add(x,y);}
        else if(!strcmp(op,"mul"))r=big_mul(x,y);
        else if(!strcmp(op,"div"))r=big_div(x,y,NULL);
        else if(!strcmp(op,"mod")){big_div(x,y,&r);}
        else if(!strcmp(op,"ceil_div")){Big rem;r=big_div(x,y,&rem);if(rem.sign)r=big_add(r,integer(u_int("1")));}
        else {fail("PRIMITIVE",name);return NULL;}
        if(natural&&r.sign<0)fail("NATURAL","natural subtraction underflow");return big_value(r);
    }
    if(!strcmp(name,"bool.not"))return u_bool(!boolean(a[0]));
    if(!strcmp(name,"bool.and")){int x=boolean(a[0]),y=boolean(a[1]);return u_bool(x&&y);}
    if(!strcmp(name,"bool.or")){int x=boolean(a[0]),y=boolean(a[1]);return u_bool(x||y);}
    if(!strncmp(name,"text.",5)||!strncmp(name,"bytes.",6)){
        int byte=name[0]=='b';const char *op=name+(byte?6:5);
        if(!strcmp(op,"from_int"))return u_text(decimal(integer(a[0])));
        if(!strcmp(op,"from_real")){char s[64];snprintf(s,sizeof(s),"%.17g",real(a[0]));return u_text(s);}
        if(!strcmp(op,"from_code")){size_t cp=index_value(a[0]);if(cp>0x10ffff)fail("UNICODE_SCALAR","codepoint outside Unicode");char s[4];size_t len=utf8((uint32_t)cp,s);return u_text_n(s,len);}
        if(!strcmp(op,"join")){Seq xs=list(a[0]);expected(a[1],K_TEXT,"join separator must be Text");TextBuffer b={0};for(size_t i=0;i<xs.n;i++){expected(xs.v[i],K_TEXT,"join items must be Text");if(i)push_bytes(&b,a[1]->as.text.s,a[1]->as.text.n);push_bytes(&b,xs.v[i]->as.text.s,xs.v[i]->as.text.n);}V v=u_text_n(b.s?b.s:"",b.n);temporary_free(b.s);return v;}
        expected(a[0],K_TEXT,"expected Text");V s=a[0];
        if(byte&&!strcmp(op,"from_hex")){
            if(s->as.text.n%2)fail("HEX","hex text requires pairs of digits");size_t length=s->as.text.n/2;char *data=allocate(length+1);
            for(size_t i=0;i<length;i++){unsigned value=0;for(size_t j=0;j<2;j++){char c=s->as.text.s[2*i+j];int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(digit<0)fail("HEX","invalid hexadecimal digit");value=value*16+(unsigned)digit;}data[i]=(char)value;}return u_text_n(data,length);
        }
        if(byte&&(!strcmp(op,"find_any")||!strcmp(op,"find"))){
            size_t start=index_value(a[2]);if(start>s->as.text.n)fail("INDEX","byte search start exceeds text");
            if(!strcmp(op,"find_any")){Seq needles=list(a[1]);unsigned char matches[256]={0};for(size_t i=0;i<needles.n;i++){size_t b=index_value(needles.v[i]);if(b>255)fail("BYTE","byte search member exceeds 255");matches[b]=1;}for(size_t i=start;i<s->as.text.n;i++)if(matches[(unsigned char)s->as.text.s[i]])return size_value(i);}
            else{expected(a[1],K_TEXT,"byte search needle must be Text");size_t n=a[1]->as.text.n;if(n<=s->as.text.n-start)for(size_t i=start;i<=s->as.text.n-n;i++)if(!memcmp(s->as.text.s+i,a[1]->as.text.s,n))return size_value(i);}
            return u_int("-1");
        }
        if(!strcmp(op,"parse_int"))return u_int(cstring(s));if(!strcmp(op,"parse_real"))return u_real(cstring(s));
        if(!strcmp(op,"eq")){expected(a[1],K_TEXT,"expected Text");return u_bool(equal(s,a[1],0));}
        if(!strcmp(op,"concat")){expected(a[1],K_TEXT,"expected Text");TextBuffer b={0};push_bytes(&b,s->as.text.s,s->as.text.n);push_bytes(&b,a[1]->as.text.s,a[1]->as.text.n);V v=u_text_n(b.s,b.n);temporary_free(b.s);return v;}
        if(!strcmp(op,"length")){if(byte)return size_value(s->as.text.n);size_t i=0,k=0;while(i<s->as.text.n){scalar(s->as.text.s,s->as.text.n,&i);k++;}return size_value(k);}
        if(!strcmp(op,"code")){size_t i=0;uint32_t cp=scalar(s->as.text.s,s->as.text.n,&i);if(i!=s->as.text.n)fail("UNICODE_SCALAR","text.code expects one scalar");return size_value(cp);}
        if(!strcmp(op,"at")){size_t i=index_value(a[1]);if(byte){if(i>=s->as.text.n)fail("INDEX","byte index out of bounds");return size_value((unsigned char)s->as.text.s[i]);}size_t start=text_offset(s,i,1);if(start==s->as.text.n)return u_text("");size_t end=start;scalar(s->as.text.s,s->as.text.n,&end);return u_text_n(s->as.text.s+start,end-start);}
        if(!strcmp(op,"slice")){size_t start=index_value(a[1]),end=index_value(a[2]);if(start>end)fail("INDEX","slice start exceeds end");if(byte){if(end>s->as.text.n)fail("INDEX","byte slice exceeds text");}else{start=text_offset(s,start,1);end=text_offset(s,end,1);}return u_text_n(s->as.text.s+start,end-start);}
    }
    if(!strncmp(name,"list.",5)){
        const char *op=name+5;Seq xs=list(a[!strcmp(op,"cons")?1:0]);
        if(!strcmp(op,"length"))return size_value(xs.n);if(!strcmp(op,"get"))return xs.v[checked_index(a[1],xs.n)];
        if(!strcmp(op,"head_or"))return xs.n?xs.v[0]:a[1];if(!strcmp(op,"tail_or_empty"))return u_list(xs.n?xs.n-1:0,xs.n?xs.v+1:NULL);
        if(!strcmp(op,"fold_left")){V state=a[1];for(size_t i=0;i<xs.n;i++)state=call2(a[2],state,xs.v[i]);return state;}
        if(!strcmp(op,"append")){Seq ys=list(a[1]);if(xs.n>SIZE_MAX-ys.n)fail("ALLOCATION_OVERFLOW","list append size");V *items=array(xs.n+ys.n,sizeof(V));memcpy(items,xs.v,xs.n*sizeof(V));memcpy(items+xs.n,ys.v,ys.n*sizeof(V));return u_list(xs.n+ys.n,items);}
        if(!strcmp(op,"cons")){if(xs.n==SIZE_MAX)fail("ALLOCATION_OVERFLOW","list cons size");V *items=array(xs.n+1,sizeof(V));items[0]=a[0];memcpy(items+1,xs.v,xs.n*sizeof(V));return u_list(xs.n+1,items);}
        V *items=array(xs.n,sizeof(V));size_t count=0;
        for(size_t i=0;i<xs.n;i++){
            if(!strcmp(op,"reverse"))items[count++]=xs.v[xs.n-i-1];
            else if(!strcmp(op,"map"))items[count++]=call1(a[1],xs.v[i]);
            else if(!strcmp(op,"filter")){if(boolean(call1(a[1],xs.v[i])))items[count++]=xs.v[i];}
            else fail("PRIMITIVE",name);
        }return u_list(count,items);
    }
    if(!strcmp(name,"tuple.make")){Seq xs=list(a[0]);return u_tuple(xs.n,xs.v);}
    if(!strcmp(name,"tuple.length")){expected(a[0],K_TUPLE,"expected Tuple");return size_value(a[0]->as.seq.n);}
    if(!strncmp(name,"tuple.",6)){expected(a[0],K_TUPLE,"expected Tuple");size_t i=!strcmp(name,"tuple.first")?0:!strcmp(name,"tuple.second")?1:checked_index(a[1],a[0]->as.seq.n);if(i>=a[0]->as.seq.n)fail("INDEX","tuple projection out of bounds");return a[0]->as.seq.v[i];}
    if(!strcmp(name,"record.make")){Seq keys=list(a[0]),values=list(a[1]);if(keys.n!=values.n)fail("RECORD_LENGTH","record keys and values must have equal lengths");const char **names=array(keys.n,sizeof(char*));for(size_t i=0;i<keys.n;i++)names[i]=cstring(keys.v[i]);return u_record(keys.n,names,values.v);}
    if(!strcmp(name,"record.put")){expected(a[0],K_RECORD,"expected Record");const char *key=cstring(a[1]);long at=field(a[0],key);size_t count=a[0]->as.record.n+(at<0?1:0);const char **keys=array(count,sizeof(char*));V *values=array(count,sizeof(V));for(size_t i=0;i<a[0]->as.record.n;i++){keys[i]=a[0]->as.record.keys[i];values[i]=a[0]->as.record.values[i];}size_t i=at<0?count-1:(size_t)at;keys[i]=key;values[i]=a[2];return u_record(count,keys,values);}
    if(!strcmp(name,"record.get"))return u_member(a[0],cstring(a[1]));
    if(!strcmp(name,"record.has"))return u_bool(field(a[0],cstring(a[1]))>=0);
    if(!strcmp(name,"record.keys")){expected(a[0],K_RECORD,"expected Record");size_t count=a[0]->as.record.n;V *items=array(count,sizeof(V));for(size_t i=0;i<count;i++)items[i]=u_text(a[0]->as.record.keys[i]);return u_list(count,items);}
    if(!strcmp(name,"cell.new")){V v=fresh(K_CELL);v->as.inner=a[0];return v;}
    if(!strcmp(name,"cell.get")){expected(a[0],K_CELL,"expected Cell");return a[0]->as.inner;}
    if(!strcmp(name,"cell.set")){expected(a[0],K_CELL,"expected Cell");if(a[1]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");a[0]->as.inner=a[1];return u_unit();}
    if(!strncmp(name,"buffer.",7)){
        const char *op=name+7;if(!strcmp(op,"new"))return fresh(K_BUFFER);expected(a[0],K_BUFFER,"expected Buffer");V b=a[0];
        if(!strcmp(op,"push")){buffer_push(b,a[1]);return u_unit();}if(!strcmp(op,"length"))return size_value(b->as.seq.n);
        if(!strcmp(op,"get"))return b->as.seq.v[checked_index(a[1],b->as.seq.n)];
        if(!strcmp(op,"set")){if(a[2]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");b->as.seq.v[checked_index(a[1],b->as.seq.n)]=a[2];return u_unit();}
        if(!strcmp(op,"freeze"))return u_list(b->as.seq.n,b->as.seq.v);
    }
    if(!strcmp(name,"value.unit"))return u_unit();
    if(!strcmp(name,"value.describe")){expected(a[0],K_CLOSURE,"expected closure");V d=a[0]->as.closure.descriptor;if(!d)fail("DESCRIPTOR","closure has no source descriptor");if(d->kind==K_TEXT){d=json_parse(d);a[0]->as.closure.descriptor=d;}return d;}
    if(!strcmp(name,"value.arity")){expected(a[0],K_CLOSURE,"expected closure");return size_value(a[0]->as.closure.arity);}
    if(!strcmp(name,"value.call")){Seq args=list(a[1]);return u_call(a[0],args.n,args.v);}
    if(!strcmp(name,"value.try_call")){
        if(pending_continuation)fail("CONTINUATION_CONTEXT","cannot install an exception boundary around a pending continuation");
        Seq args=list(a[1]);expected(a[0],K_CLOSURE,"try_call requires a closure");struct ErrorFrame *frame=malloc(sizeof(*frame));if(!frame)fail("ALLOCATION","exception boundary allocation failed");
        frame->previous=error_frame;frame->depth=call_depth;frame->pending=pending_continuation;frame->forcing=forcing_bindings;frame->temporary=temporaries;error_frame=frame;
        if(!setjmp(frame->jump)){
            V result=u_call(a[0],args.n,args.v);temporary_cleanup(frame->temporary);error_frame=frame->previous;free(frame);const char *keys[]={"ok","value"};V values[]={u_bool(1),result};return u_record(2,keys,values);
        }
        error_frame=frame->previous;const char *keys[]={"ok","code","message"};V values[]={u_bool(0),u_text(frame->code),u_text(frame->message)};free(frame);return u_record(3,keys,values);
    }
    if(!strncmp(name,"table.",6)){
        if(!strcmp(name,"table.new")){V table=fresh(K_TABLE);table_grow(table);return table;}
        expected(a[0],K_TABLE,"expected Table");V table=a[0];uint64_t hash=text_hash(a[1]);
        if(!strcmp(name,"table.set")){
            if(a[2]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be stored");
            if(table->as.table.n>=table->as.table.cap/2)table_grow(table);Slot *slot=table_slot(table,a[1],hash);
            if(!slot->key){slot->key=a[1]->as.text.s;slot->length=a[1]->as.text.n;slot->hash=hash;table->as.table.n++;}slot->value=a[2];return u_unit();
        }
        Slot *slot=table_slot(table,a[1],hash);if(!strcmp(name,"table.has"))return u_bool(slot->key!=NULL);return slot->key?slot->value:a[2];
    }
    if(!strncmp(name,"resource.",9)){
        if(!strcmp(name,"resource.new")){V v=fresh(K_RESOURCE);v->as.resource.kind=cstring(a[0]);v->as.resource.payload=a[1];v->as.resource.live=1;v->as.resource.owner_pid=getpid();return v;}
        expected(a[0],K_RESOURCE,"expected opaque resource");V r=a[0];if(!r->as.resource.live)fail("RESOURCE_CONSUMED","resource is no longer live");
        if(r->as.resource.owner_pid!=getpid())fail("RESOURCE_PROCESS","opaque resource authority cannot cross an isolated worker boundary");
        if(!strcmp(name,"resource.kind"))return u_text(r->as.resource.kind);
        if(strcmp(r->as.resource.kind,cstring(a[1])))fail("RESOURCE_KIND","resource kind does not match access contract");
        if(!strcmp(name,"resource.take"))r->as.resource.live=0;return r->as.resource.payload;
    }
    if(!strcmp(name,"value.select"))return boolean(a[0])?a[1]:a[2];
    if(!strcmp(name,"value.equal"))return u_bool(equal(a[0],a[1],0));
    if(!strcmp(name,"value.kind")){static const char *kinds[]={"unit","bool","int","real","text","list","tuple","record","closure","namespace","cell","buffer","partial","continuation","resource","table","process","future"};if(a[0]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation is not ordinary data");return u_text(kinds[a[0]->kind]);}
    if(!strcmp(name,"value.fail")){fail("USER",cstring(a[0]));}
    if(!strcmp(name,"json.encode")||!strcmp(name,"value.to_text"))return json_encode(a[0]);
    if(!strcmp(name,"json.parse"))return json_parse(a[0]);
    if(!strcmp(name,"control.while")){while(boolean(call0(a[0])))call0(a[1]);return u_unit();}
    if(!strcmp(name,"partial.done")){if(a[0]->kind==K_AGAIN)fail("CONTINUATION_CONTEXT","continuation cannot be wrapped");V v=fresh(K_PARTIAL);v->as.inner=a[0];return v;}
    if(!strcmp(name,"partial.value")){expected(a[0],K_PARTIAL,"expected completed Partial");return a[0]->as.inner;}
    if(!strcmp(name,"core.fix_partial")){
        V state=a[1],again=u_closure(again_call,NULL,1);
        for(;;){V result=call2(a[0],again,state);if(result->kind==K_AGAIN){if(pending_continuation!=result)fail("CONTINUATION_CONTEXT","invalid continuation ownership");pending_continuation=NULL;state=result->as.inner;}else{expected(result,K_PARTIAL,"fixed point body returns Partial or direct continuation");return result;}}
    }
    if(!strcmp(name,"sys.args")){V *items=array((size_t)runtime_argc,sizeof(V));for(int i=0;i<runtime_argc;i++)items[i]=u_text(runtime_argv[i]);return u_list((size_t)runtime_argc,items);}
    if(!strcmp(name,"sys.native_names")){V *items=array(PRIMITIVE_COUNT*2,sizeof(V));for(size_t i=0;i<PRIMITIVE_COUNT;i++){items[i*2]=u_text(primitives[i].name);items[i*2+1]=u_text(native_alias(primitives[i].name));}return u_list(PRIMITIVE_COUNT*2,items);}
    if(!strcmp(name,"sys.getenv")){const char *key=cstring(a[0]);if(strcmp(key,"U_ROOT")&&strcmp(key,"U_USER_ARGS_JSON")&&strcmp(key,"U_NATIVE_LINK_CRYPTO"))capability("env");const char *value=getenv(key);return u_text(value?value:"");}
    if(!strcmp(name,"sys.has_cap"))return u_bool(capability_granted(cstring(a[0])));
    if(!strcmp(name,"capability.require")){capability(cstring(a[0]));return u_unit();}
    if(!strcmp(name,"sys.stdout_write")){capability("console");expected(a[0],K_TEXT,"stdout write requires Text");if(fwrite(a[0]->as.text.s,1,a[0]->as.text.n,stdout)!=a[0]->as.text.n||fflush(stdout))fail("IO_WRITE","stdout write failed");return u_unit();}
    if(!strcmp(name,"sys.stdin_line")){
        TextBuffer line={0};int c;
        while((c=fgetc(stdin))!=EOF&&c!='\n'){if(line.n>=1024*1024)fail("INPUT_LIMIT","stdin line exceeds 1 MiB");char ch=(char)c;push_bytes(&line,&ch,1);}
        if(ferror(stdin))fail("IO_READ","stdin read failed");if(c==EOF&&!line.n){temporary_free(line.s);return u_unit();}
        if(line.n&&line.s[line.n-1]=='\r')line.n--;V value=u_text_n(line.s?line.s:"",line.n);temporary_free(line.s);return value;
    }
    if(!strcmp(name,"sys.stdin_read")){
        size_t wanted=index_value(a[0]);if(wanted>8*1024*1024)fail("INPUT_LIMIT","stdin read exceeds 8 MiB");if(!wanted)return u_text("");char *data=allocate(wanted+1);size_t count=0;
        while(count<wanted){size_t got=fread(data+count,1,wanted-count,stdin);count+=got;if(!got){if(ferror(stdin))fail("IO_READ","stdin read failed");break;}}
        return count?u_text_n(data,count):u_unit();
    }
    if(!strcmp(name,"sys.file_kind")){capability("read");return u_text(file_kind(cstring(a[0])));}
    if(!strcmp(name,"sys.cwd")){
        capability("read");size_t capacity=128;for(;;){char *path=temporary_malloc(capacity);if(getcwd(path,capacity)){V result=u_text(path);temporary_free(path);return result;}int error=errno;temporary_free(path);if(error!=ERANGE||capacity>=8*1024*1024)fail("CWD","working directory cannot be represented within the native limit");capacity*=2;}
    }
    if(!strcmp(name,"sys.mkdir")){capability("write");if(mkdir(cstring(a[0]),0777))fail("MKDIR",strerror(errno));return u_unit();}
    if(!strcmp(name,"sys.rename")){capability("write");if(rename(cstring(a[0]),cstring(a[1])))fail("RENAME",strerror(errno));return u_unit();}
    if(!strcmp(name,"sys.list_dir")){
        capability("read");const char *path=cstring(a[0]);DIR *directory=opendir(path);if(!directory)fail("DIRECTORY",strerror(errno));temporary_resource(directory,release_directory);V entries=fresh(K_BUFFER);struct dirent *entry;
        for(;;){errno=0;entry=readdir(directory);if(!entry){if(errno)fail("DIRECTORY",strerror(errno));break;}if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
            if(entries->as.seq.n>=100000)fail("DIRECTORY_LIMIT","one-level directory listing exceeds 100000 entries");TextBuffer full={0};push(&full,path);if(full.n&&full.s[full.n-1]!='/')push(&full,"/");push(&full,entry->d_name);
            const char *keys[]={"name","kind"};V values[]={u_text(entry->d_name),u_text(file_kind(full.s))};temporary_free(full.s);buffer_push(entries,u_record(2,keys,values));
        }
        temporary_forget(directory);if(closedir(directory))fail("DIRECTORY",strerror(errno));return u_list(entries->as.seq.n,entries->as.seq.v);
    }
    if(!strcmp(name,"sys.write_bytes")||!strcmp(name,"sys.write_bytes_new")){
        capability("write");const char *path=cstring(a[0]);Seq bytes=list(a[1]);unsigned char *data=array(bytes.n,1);for(size_t i=0;i<bytes.n;i++){size_t byte=index_value(bytes.v[i]);if(byte>255)fail("BYTE","binary output requires integers in 0..255");data[i]=(unsigned char)byte;}
        FILE *file=open_output(path,!strcmp(name,"sys.write_bytes_new"));size_t count=fwrite(data,1,bytes.n,file);temporary_forget(file);int status=fclose(file);if(count!=bytes.n||status)fail("IO_WRITE","binary file write failed");return u_unit();
    }
    if(!strcmp(name,"process.spawn")||!strcmp(name,"process.spawn_with_caps")){
        for(struct ProcessRecord *r=process_records;r;r=r->next)process_reap(&r->handle->as.process);
        TextBuffer grants={0};if(!strcmp(name,"process.spawn_with_caps")){Seq caps=list(a[1]);if(caps.n>32)fail("CAPABILITY","worker grant list exceeds 32 entries");for(size_t i=0;i<caps.n;i++){const char *grant=cstring(caps.v[i]);capability(grant);if(i)push(&grants,",");push(&grants,grant);}}
        expected(a[0],K_CLOSURE,"worker action must be a closure");arity(a[0]->as.closure.arity,0);if(active_processes>=64)fail("PROCESS_LIMIT","at most 64 active isolated workers are admitted");
        int fds[2];if(pipe(fds))fail("PROCESS_PIPE",strerror(errno));FILE *temporary=tmpfile();if(!temporary)fail("PROCESS_STORAGE",strerror(errno));int result_fd=dup(fileno(temporary));fclose(temporary);if(result_fd<0)fail("PROCESS_STORAGE",strerror(errno));
        FILE *shared_file=tmpfile();if(!shared_file||ftruncate(fileno(shared_file),(off_t)sizeof(ProcessShared)))fail("PROCESS_SHARED",strerror(errno));
        ProcessShared *shared=mmap(NULL,sizeof(*shared),PROT_READ|PROT_WRITE,MAP_SHARED,fileno(shared_file),0);fclose(shared_file);if(shared==MAP_FAILED)fail("PROCESS_SHARED",strerror(errno));atomic_init(&shared->result_ready,0);atomic_init(&shared->failed,0);atomic_init(&shared->cancelled,0);
        fcntl(fds[0],F_SETFD,FD_CLOEXEC);fcntl(fds[1],F_SETFD,FD_CLOEXEC);fcntl(result_fd,F_SETFD,FD_CLOEXEC);
        fflush(NULL);pid_t pid=fork();if(pid<0){close(fds[0]);close(fds[1]);close(result_fd);munmap(shared,sizeof(*shared));fail("PROCESS_FORK",strerror(errno));}
        if(!pid){
            close(fds[0]);if(current_worker_completion_fd>=0)close(current_worker_completion_fd);current_worker_completion_fd=fds[1];current_worker_shared=shared;process_records=NULL;active_processes=0;error_frame=NULL;forcing_bindings=NULL;
            unsetenv("U_NATIVE_ALLOW");if(grants.n&&setenv("U_NATIVE_ALLOW",grants.s,1))fail("CAPABILITY","worker grant setup failed");temporary_free(grants.s);
            V result=call0(a[0]);terminate_owned_processes();if(shared_call_budget&&atomic_load(shared_call_budget+1))fail("BUDGET_EXHAUSTED","shared worker call-dispatch allowance exhausted");
            V text=json_encode(transport_pack(result,0));if(text->as.text.n>64*1024*1024)fail("TRANSPORT_LIMIT","worker result exceeds 64 MiB");size_t sent=0;
            while(sent<text->as.text.n){ssize_t n=pwrite(result_fd,text->as.text.s+sent,text->as.text.n-sent,(off_t)sent);if(n<0&&errno==EINTR)continue;if(n<=0)fail("PROCESS_STORAGE","worker result write failed");sent+=(size_t)n;}
            atomic_store(&shared->result_ready,1);fflush(NULL);close(fds[1]);close(result_fd);_exit(0);
        }
        temporary_free(grants.s);close(fds[1]);V handle=fresh(K_PROCESS);handle->as.process.pid=pid;handle->as.process.owner=getpid();handle->as.process.fd=fds[0];handle->as.process.result_fd=result_fd;handle->as.process.shared=shared;
        struct ProcessRecord *record=allocate(sizeof(*record));record->handle=handle;record->next=process_records;process_records=record;active_processes++;return handle;
    }
    if(!strcmp(name,"process.poll"))return process_poll(a[0]);
    if(!strcmp(name,"process.sleep")){size_t ms=index_value(a[0]);if(ms>86400000)fail("PROCESS_SLEEP","sleep is limited to one day per call");struct timespec delay={(time_t)(ms/1000),(long)((ms%1000)*1000000)};while(nanosleep(&delay,&delay)<0)if(errno!=EINTR)fail("PROCESS_SLEEP",strerror(errno));return u_unit();}
    if(!strcmp(name,"process.share")){V future=fresh(K_FUTURE);future->as.future=owned_process(a[0]);return future;}
    if(!strcmp(name,"process.observe")){expected(a[0],K_FUTURE,"expected shared result reading right");return process_observe(a[0]->as.future);}
    if(!strcmp(name,"process.cancel")){Process *p=owned_process(a[0]);process_observe(p);if(p->outcome)return u_bool(0);atomic_store(&p->shared->cancelled,1);return u_bool(kill(p->pid,SIGTERM)==0);}
    if(!strcmp(name,"process.wait")||!strcmp(name,"process.result")){
        Process *p;if(!strcmp(name,"process.wait"))p=owned_process(a[0]);else{expected(a[0],K_FUTURE,"expected shared result reading right");p=a[0]->as.future;}
        Big limit=integer(a[1]);int unlimited=compare(limit,integer(u_int("-1")))==0;if(limit.sign<0&&!unlimited)fail("PROCESS_TIMEOUT","timeout must be -1 or nonnegative milliseconds");double deadline=unlimited?0:monotonic_ms()+(double)index_value(a[1]);
        for(;;){V result=process_observe(p);if(p->outcome||(!unlimited&&monotonic_ms()>=deadline))return result;struct pollfd fd={p->fd,POLLIN|POLLHUP,0};int delay=20;if(!unlimited&&deadline-monotonic_ms()<delay)delay=(int)fmax(0,deadline-monotonic_ms());if(poll(&fd,1,delay)<0&&errno!=EINTR)fail("PROCESS_POLL",strerror(errno));}
    }
    if(!strcmp(name,"crypto.sha256")){
        expected(a[0],K_TEXT,"SHA-256 input must be UTF-8 Text");unsigned char digest[32];
#ifdef __APPLE__
        if(a[0]->as.text.n>UINT_MAX)fail("HASH_LIMIT","CommonCrypto input exceeds one-shot length bound");
        CC_SHA256(a[0]->as.text.s,(CC_LONG)a[0]->as.text.n,digest);
#else
        if(!SHA256((const unsigned char*)a[0]->as.text.s,a[0]->as.text.n,digest))fail("HASH","OpenSSL SHA-256 failed");
#endif
        const char *hex="0123456789abcdef";char result[65];for(size_t i=0;i<32;i++){result[2*i]=hex[digest[i]>>4];result[2*i+1]=hex[digest[i]&15];}result[64]=0;return u_text(result);
    }
    if(!strcmp(name,"crypto.sha256_file")){
        capability("read");const char *path=cstring(a[0]);int fd=open(path,O_RDONLY|O_NONBLOCK);if(fd<0)fail("HASH_FILE",strerror(errno));struct stat info;if(fstat(fd,&info)||!S_ISREG(info.st_mode)){close(fd);fail("HASH_FILE","SHA-256 file input must be a regular file");}
        FILE *file=fdopen(fd,"rb");if(!file){close(fd);fail("HASH_FILE",strerror(errno));}temporary_resource(file,release_file);unsigned char digest[32],chunk[65536];size_t count;
#ifdef __APPLE__
        CC_SHA256_CTX context;if(!CC_SHA256_Init(&context))fail("HASH","CommonCrypto SHA-256 initialization failed");
        while((count=fread(chunk,1,sizeof(chunk),file)))if(!CC_SHA256_Update(&context,chunk,(CC_LONG)count))fail("HASH","CommonCrypto SHA-256 update failed");
        if(ferror(file)||!CC_SHA256_Final(digest,&context))fail("HASH_FILE","SHA-256 file read or finalization failed");
#else
        EVP_MD_CTX *context=EVP_MD_CTX_new();if(!context)fail("HASH","OpenSSL digest context allocation failed");temporary_resource(context,release_digest);if(!EVP_DigestInit_ex(context,EVP_sha256(),NULL))fail("HASH","OpenSSL SHA-256 initialization failed");
        while((count=fread(chunk,1,sizeof(chunk),file)))if(!EVP_DigestUpdate(context,chunk,count))fail("HASH","OpenSSL SHA-256 update failed");
        unsigned int length=0;if(ferror(file)||!EVP_DigestFinal_ex(context,digest,&length)||length!=32)fail("HASH_FILE","SHA-256 file read or finalization failed");temporary_free(context);
#endif
        temporary_free(file);const char *hex="0123456789abcdef";char result[65];for(size_t i=0;i<32;i++){result[2*i]=hex[digest[i]>>4];result[2*i+1]=hex[digest[i]&15];}result[64]=0;return u_text(result);
    }
    if(!strcmp(name,"http.transport")){
        capability("network");const char *url=cstring(a[0]);if(strncmp(url,"https://",8)&&strncmp(url,"http://",7))fail("HTTP_URL","explicit http or https URL required");
        FILE *body=tmpfile();if(!body)fail("HTTP_STORAGE",strerror(errno));temporary_resource(body,release_file);int body_fd=fileno(body),status_pipe[2];if(pipe(status_pipe))fail("HTTP_PIPE",strerror(errno));
        char output[80];snprintf(output,sizeof(output),"/dev/fd/%d",body_fd);fflush(NULL);pid_t pid=fork();if(pid<0)fail("HTTP_FORK",strerror(errno));
        if(!pid){close(status_pipe[0]);if(dup2(status_pipe[1],STDOUT_FILENO)<0)_exit(74);close(status_pipe[1]);fcntl(body_fd,F_SETFD,0);unsetenv("U_NATIVE_ALLOW");
            char *args[]={"/usr/bin/curl","--disable","--silent","--show-error","--location","--max-redirs","5","--max-time","15","--connect-timeout","5","--max-filesize","8388608","--proto","=http,https","--proto-redir","=http,https","--proxy","","--output",output,"--write-out","%{http_code}\n%{content_type}","--url",(char*)url,NULL};
            char *transport_env[]={"PATH=/usr/bin:/bin","LANG=C",NULL};execve(args[0],args,transport_env);_exit(127);
        }
        close(status_pipe[1]);TextBuffer status={0};char chunk[1024];for(;;){ssize_t got=read(status_pipe[0],chunk,sizeof(chunk));if(got<0&&errno==EINTR)continue;if(got<0)fail("HTTP_READ",strerror(errno));if(!got)break;if(status.n+(size_t)got>8192){kill(pid,SIGTERM);fail("HTTP_HEADERS","HTTP metadata exceeds bound");}push_bytes(&status,chunk,(size_t)got);}close(status_pipe[0]);int code;while(waitpid(pid,&code,0)<0)if(errno!=EINTR)fail("HTTP_WAIT",strerror(errno));
        if(!WIFEXITED(code)||WEXITSTATUS(code)!=0){temporary_free(body);temporary_free(status.s);fail("HTTP_TRANSPORT","curl request failed or exceeded its timeout/size contract");}
        if(status.n<4||status.s[3]!='\n')fail("HTTP_STATUS","malformed transport status");V response_status=u_int(copy_bytes(status.s,3));
        struct stat info;if(fstat(body_fd,&info)||info.st_size<0||info.st_size>8*1024*1024)fail("HTTP_SIZE","response body exceeds 8 MiB");size_t size=(size_t)info.st_size;char *hex=allocate(size*2+1);const char *digits="0123456789abcdef";size_t at=0;
        while(at<size){ssize_t got=pread(body_fd,chunk,sizeof(chunk),(off_t)at);if(got<0&&errno==EINTR)continue;if(got<=0)fail("HTTP_READ","incomplete response body");for(ssize_t i=0;i<got;i++){unsigned char byte=(unsigned char)chunk[i];hex[2*(at+(size_t)i)]=digits[byte>>4];hex[2*(at+(size_t)i)+1]=digits[byte&15];}at+=(size_t)got;}
        temporary_free(body);const char *keys[]={"status","body_hex","content_type","transport"};V values[]={response_status,u_text_n(hex,size*2),u_text_n(status.s+4,status.n-4),u_text("system_curl.http_https")};temporary_free(status.s);return u_record(4,keys,values);
    }
    if(!strcmp(name,"sys.print")){capability("console");expected(a[0],K_TEXT,"sys.print expects Text");if(fwrite(a[0]->as.text.s,1,a[0]->as.text.n,stdout)!=a[0]->as.text.n||fputc('\n',stdout)==EOF)fail("IO_WRITE","stdout write failed");return u_unit();}
    if(!strcmp(name,"sys.exit")){size_t code=index_value(a[0]);if(code>255)fail("EXIT_CODE","exit status must be 0 through 255");terminate_owned_processes();exit((int)code);}
    if(!strcmp(name,"sys.read")){
        capability("read");
        FILE *f=fopen(cstring(a[0]),"rb");if(!f)fail("IO_READ",strerror(errno));TextBuffer b={0};char chunk[8192];size_t count;
        while((count=fread(chunk,1,sizeof(chunk),f)))push_bytes(&b,chunk,count);if(ferror(f)){fclose(f);fail("IO_READ","file read failed");}fclose(f);V v=u_text_n(b.s?b.s:"",b.n);temporary_free(b.s);return v;
    }
    if(!strcmp(name,"sys.write")||!strcmp(name,"sys.write_new")){
        capability("write");
        const char *path=cstring(a[0]);expected(a[1],K_TEXT,"sys.write contents must be Text");FILE *f=open_output(path,!strcmp(name,"sys.write_new"));size_t count=fwrite(a[1]->as.text.s,1,a[1]->as.text.n,f);temporary_forget(f);int closed=fclose(f);if(count!=a[1]->as.text.n||closed)fail("IO_WRITE","file write failed");return u_unit();
    }
    if(!strcmp(name,"sys.exec")){
        capability("exec");
        Seq xs=list(a[0]);if(!xs.n)fail("PROCESS_ARGV","process argv must not be empty");char **args=array(xs.n+1,sizeof(char*));for(size_t i=0;i<xs.n;i++)args[i]=(char*)cstring(xs.v[i]);
        pid_t pid=fork();if(pid<0)fail("PROCESS_FORK",strerror(errno));if(!pid){unsetenv("U_NATIVE_ALLOW");execvp(args[0],args);fprintf(stderr,"U_NATIVE_ERROR PROCESS_EXEC: %s\n",strerror(errno));_exit(127);}int status;
        while(waitpid(pid,&status,0)<0)if(errno!=EINTR)fail("PROCESS_WAIT",strerror(errno));return size_value(WIFEXITED(status)?(size_t)WEXITSTATUS(status):(size_t)(128+WTERMSIG(status)));
    }
    if(!strncmp(name,"f64.",4)||!strncmp(name,"f32.",4)){
        const char *op=name+4;int single=name[1]=='3';double x=real(a[0]);if(single)x=(double)(float)x;
        if(!strcmp(op,"bits")){if(!isfinite(x))fail("REAL_FINITE","canonical floating-point bits require finite binary64");uint64_t bits;memcpy(&bits,&x,sizeof(bits));char text[17];snprintf(text,sizeof(text),"%016" PRIx64,bits);return u_text(text);}
        if(!strcmp(op,"from_real"))return real_value(x);double y=real(a[1]);if(single)y=(double)(float)y;
        if(!strcmp(op,"eq"))return u_bool(x==y);if(!strcmp(op,"lt"))return u_bool(x<y);if(!strcmp(op,"le"))return u_bool(x<=y);if(!strcmp(op,"gt"))return u_bool(x>y);if(!strcmp(op,"ge"))return u_bool(x>=y);
        double result;if(!strcmp(op,"add"))result=x+y;else if(!strcmp(op,"sub"))result=x-y;else if(!strcmp(op,"mul"))result=x*y;else if(!strcmp(op,"div"))result=x/y;else{fail("PRIMITIVE",name);return NULL;}return real_value(single?(double)(float)result:result);
    }
    if(!strncmp(name,"math.",5)){
        double x=real(a[0]);const char *op=name+5;
        if(!strcmp(op,"is_finite"))return u_bool(isfinite(x));
        if(!strcmp(op,"sin"))return real_value(sin(x));if(!strcmp(op,"cos"))return real_value(cos(x));if(!strcmp(op,"exp"))return real_value(exp(x));if(!strcmp(op,"log"))return real_value(log(x));if(!strcmp(op,"sqrt"))return real_value(sqrt(x));if(!strcmp(op,"abs"))return real_value(fabs(x));if(!strcmp(op,"fma"))return real_value(fma(x,real(a[1]),real(a[2])));
    }
    if(!strcmp(name,"random.uniform")){
        uint64_t seed=(uint64_t)index_value(a[0]),index=(uint64_t)index_value(a[1]);uint64_t z=seed+UINT64_C(0x9e3779b97f4a7c15)*(index+1);z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb);z^=z>>31;return real_value((double)(z>>11)*0x1.0p-53);
    }
    if(!strcmp(name,"sum.make")){cstring(a[0]);const char *keys[]={"tag","value"};return u_record(2,keys,a);}
    if(!strcmp(name,"sum.case")){V tag=u_member(a[0],"tag");V body=u_member(a[1],cstring(tag));return call1(body,u_member(a[0],"value"));}
    if(!strcmp(name,"option.when")){int present=boolean(a[0]);const char *keys[]={"present","value"};V values[]={u_bool(present),present?call0(a[1]):u_unit()};return u_record(2,keys,values);}
    fail("PRIMITIVE",name);return NULL;
}
void u_runtime_init(int argc,char **argv){
    runtime_argc=argc;runtime_argv=argv;const char *memory=getenv("U_NATIVE_MEMORY_MB");
    if(memory&&*memory){char *end;errno=0;unsigned long long mb=strtoull(memory,&end,10);if(errno||*end||!mb||mb>SIZE_MAX/(1024*1024))fail("MEMORY_CONFIG","invalid U_NATIVE_MEMORY_MB");allocation_limit=(size_t)mb*1024*1024;}
    const char *steps=getenv("U_NATIVE_STEPS");if(steps&&*steps){
        char *end;errno=0;unsigned long long count=strtoull(steps,&end,10);if(errno||*end||*steps=='-')fail("BUDGET_CONFIG","U_NATIVE_STEPS must be an unsigned integer");
        FILE *file=tmpfile();if(!file||ftruncate(fileno(file),(off_t)(2*sizeof(*shared_call_budget))))fail("BUDGET_STORAGE","shared dispatch budget allocation failed");
        shared_call_budget=mmap(NULL,2*sizeof(*shared_call_budget),PROT_READ|PROT_WRITE,MAP_SHARED,fileno(file),0);fclose(file);if(shared_call_budget==MAP_FAILED)fail("BUDGET_STORAGE","shared dispatch budget mapping failed");atomic_init(shared_call_budget,(uint64_t)count);atomic_init(shared_call_budget+1,0);
    }
}
V u_entry(E root,const char *name){
    V f=u_lookup(root,name);
    const char *args=getenv("U_ARGS_JSON");
    if(f->kind!=K_CLOSURE){if(args&&list(json_parse(u_text(args))).n)fail("ARITY","constant entry does not accept arguments");return f;}
    if(!args&&f->as.closure.arity&&runtime_argc>1)args=runtime_argv[1];
    if(args){Seq xs=list(json_parse(u_text(args)));return u_call(f,xs.n,xs.v);}return call0(f);
}
int u_finish(V result){
    terminate_owned_processes();
    if(shared_call_budget&&atomic_load(shared_call_budget+1))fail("BUDGET_EXHAUSTED","shared worker call-dispatch allowance exhausted");
    if(result->kind!=K_UNIT){V text=json_encode(result);if(fwrite(text->as.text.s,1,text->as.text.n,stdout)!=text->as.text.n||fputc('\n',stdout)==EOF)fail("IO_WRITE","result output failed");}
    if(shared_call_budget)munmap(shared_call_budget,2*sizeof(*shared_call_budget));
    temporary_cleanup(NULL);
    while(allocations){struct Allocation *next=allocations->next;free(allocations);allocations=next;}allocated=0;return 0;
}
