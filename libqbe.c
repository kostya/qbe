#include <stdarg.h>
#include <setjmp.h>

#include "all.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

Target T;
char debug['Z'+1] = {
    ['P'] = 0, ['M'] = 0, ['N'] = 0, ['C'] = 0,
    ['G'] = 0, ['K'] = 0, ['A'] = 0, ['I'] = 0,
    ['L'] = 0, ['S'] = 0, ['R'] = 0,
};

static jmp_buf error_jmp;
static int error_jmp_valid = 0;
static char error_buf[256] = {0};
extern char *inpath;
extern int lnum;

void
die_(char *file, char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vsnprintf(error_buf, sizeof(error_buf), msg, ap);
    va_end(ap);
    
    if (error_jmp_valid) {
        error_jmp_valid = 0;
        longjmp(error_jmp, 1);
    } else {
        fprintf(stderr, "%s: dying: %s\n", file, error_buf);
        abort();
    }
}

void
err(char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);

    int len = snprintf(error_buf, sizeof(error_buf), "%s:%d: ", inpath, lnum);
    vsnprintf(error_buf + len, sizeof(error_buf) - len, msg, ap);
    va_end(ap);
    
    if (error_jmp_valid) {
        error_jmp_valid = 0;
        longjmp(error_jmp, 1);
    } else {
        fprintf(stderr, "qbe: %s\n", error_buf);
        exit(1);
    }
}

typedef struct QBE {
    Target target;
    FILE *outf;
    int dbg;
    int initialized;
    char error[256];
} QBE;

extern Target T_amd64_sysv, T_amd64_apple, T_amd64_win;
extern Target T_arm64, T_arm64_apple, T_rv64;

static Target *tlist[] = {
    &T_amd64_sysv, &T_amd64_apple, &T_amd64_win,
    &T_arm64, &T_arm64_apple, &T_rv64, 0
};

typedef struct {
    FILE *outf;
    int dbg;
} ParseContext;

static ParseContext parse_ctx;

static void data_cb(Dat *d) {
    if (!parse_ctx.dbg) {
        emitdat(d, parse_ctx.outf);
        if (d->type == DEnd) {
            fputs("/* end data */\n\n", parse_ctx.outf);
            freeall();
        }
    }
}

static void func_cb(Fn *fn) {
    uint n;
    
    if (parse_ctx.dbg)
        fprintf(stderr, "**** Function %s ****", fn->name);
    
    if (debug['P']) {
        fprintf(stderr, "\n> After parsing:\n");
        printfn(fn, stderr);
    }
    
    T.abi0(fn);
    fillcfg(fn);
    filluse(fn);
    promote(fn);
    filluse(fn);
    ssa(fn);
    filluse(fn);
    ssacheck(fn);
    fillalias(fn);
    loadopt(fn);
    filluse(fn);
    fillalias(fn);
    coalesce(fn);
    filluse(fn);
    filldom(fn);
    ssacheck(fn);
    gvn(fn);
    fillcfg(fn);
    simplcfg(fn);
    filluse(fn);
    filldom(fn);
    gcm(fn);
    filluse(fn);
    ssacheck(fn);
    
    if (T.cansel) {
        ifconvert(fn);
        fillcfg(fn);
        filluse(fn);
        filldom(fn);
        ssacheck(fn);
    }
    
    T.abi1(fn);
    simpl(fn);
    fillcfg(fn);
    filluse(fn);
    T.isel(fn);
    fillcfg(fn);
    filllive(fn);
    fillloop(fn);
    fillcost(fn);
    spill(fn);
    rega(fn);
    fillcfg(fn);
    simpljmp(fn);
    fillcfg(fn);
    
    assert(fn->rpo[0] == fn->start);
    for (n = 0;; n++)
        if (n == fn->nblk - 1) {
            fn->rpo[n]->link = 0;
            break;
        } else
            fn->rpo[n]->link = fn->rpo[n + 1];
    
    if (!parse_ctx.dbg) {
        T.emitfn(fn, parse_ctx.outf);
        fprintf(parse_ctx.outf, "/* end function %s */\n\n", fn->name);
    } else
        fprintf(stderr, "\n");
    
    freeall();
}

static void dbgfile_cb(char *fn) {
    emitdbgfile(fn, parse_ctx.outf);
}

QBE *qbe_new(void) {
    QBE *q = calloc(1, sizeof(QBE));
    if (!q) return NULL;
    return q;
}

void qbe_free(QBE *q) {
    if (q) {
        if (q->outf && q->outf != stdout)
            fclose(q->outf);
        free(q);
    }
}

int qbe_init_defaults(QBE *q) {
    if (!q) return -1;
    
    q->target = Deftgt;
    q->outf = stdout;
    q->dbg = 0;
    q->initialized = 1;
    q->error[0] = '\0';
    
    T = Deftgt;
    
    return 0;
}

int qbe_set_target(QBE *q, const char *target_name) {
    if (!q || !target_name) return -1;
    
    for (Target **t = tlist; *t; t++) {
        if (strcmp(target_name, (*t)->name) == 0) {
            q->target = **t;
            return 0;
        }
    }
    
    snprintf(q->error, sizeof(q->error), 
             "unknown target '%s'", target_name);
    return -1;
}

int qbe_set_output_file(QBE *q, const char *filename) {
    if (!q || !filename) return -1;
    
    if (q->outf && q->outf != stdout)
        fclose(q->outf);
    
    if (strcmp(filename, "-") == 0) {
        q->outf = stdout;
    } else {
        q->outf = fopen(filename, "w");
        if (!q->outf) {
            snprintf(q->error, sizeof(q->error),
                     "cannot open '%s'", filename);
            return -1;
        }
    }
    return 0;
}

void qbe_set_debug_flags(QBE *q, const char *flags) {
    if (!q || !flags) return;
    
    for (; *flags; flags++) {
        if (isalpha(*flags)) {
            debug[toupper(*flags)] = 1;
            q->dbg = 1;
        }
    }
}

static int do_compile(QBE *q, FILE *stream, char *name) {
    Target saved_T = T;
    
    T = q->target;
    parse_ctx.outf = q->outf;
    parse_ctx.dbg = q->dbg;
    
    parse(stream, name, dbgfile_cb, data_cb, func_cb);
    
    T = saved_T;
    return 0;
}

int qbe_compile_string(QBE *q, const char *input, const char *name) {
    if (!q || !input) return -1;
    
    char *name_copy = name ? strdup(name) : strdup("<string>");
    if (!name_copy) {
        snprintf(q->error, sizeof(q->error), "memory error");
        return -1;
    }
    
    FILE *stream = fmemopen((void*)input, strlen(input), "r");
    if (!stream) {
        snprintf(q->error, sizeof(q->error), "memory error");
        free(name_copy);
        return -1;
    }
    
    error_jmp_valid = 1;
    if (setjmp(error_jmp) == 0) {
        do_compile(q, stream, name_copy);
        error_jmp_valid = 0;
        fclose(stream);
        free(name_copy);
        return 0;
    } else {
        snprintf(q->error, sizeof(q->error), "%s", error_buf);
        fclose(stream);
        free(name_copy);
        return -1;
    }
}

int qbe_compile_file(QBE *q, const char *filename) {
    if (!q || !filename) return -1;
    
    char *name_copy = strdup(filename);
    if (!name_copy) {
        snprintf(q->error, sizeof(q->error), "memory error");
        return -1;
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        snprintf(q->error, sizeof(q->error),
                 "cannot open '%s'", filename);
        free(name_copy);
        return -1;
    }
    
    error_jmp_valid = 1;
    if (setjmp(error_jmp) == 0) {
        do_compile(q, f, name_copy);
        error_jmp_valid = 0;
        fclose(f);
        free(name_copy);
        return 0;
    } else {
        snprintf(q->error, sizeof(q->error), "%s", error_buf);
        fclose(f);
        free(name_copy);
        return -1;
    }
}

int qbe_compile_stream(QBE *q, FILE *stream, const char *name) {
    if (!q || !stream) return -1;
    
    char *name_copy = name ? strdup(name) : strdup("<stream>");
    if (!name_copy) {
        snprintf(q->error, sizeof(q->error), "memory error");
        return -1;
    }
    
    error_jmp_valid = 1;
    if (setjmp(error_jmp) == 0) {
        do_compile(q, stream, name_copy);
        error_jmp_valid = 0;
        free(name_copy);
        return 0;
    } else {
        snprintf(q->error, sizeof(q->error), "%s", error_buf);
        free(name_copy);
        return -1;
    }
}

int qbe_finish(QBE *q) {
    if (!q) return -1;
    
    Target saved_T = T;
    T = q->target;
    
    if (!q->dbg) {
        error_jmp_valid = 1;
        if (setjmp(error_jmp) == 0) {
            T.emitfin(q->outf);
            error_jmp_valid = 0;
        } else {
            snprintf(q->error, sizeof(q->error), "%s", error_buf);
            T = saved_T;
            return -1;
        }
    }
    
    T = saved_T;
    return 0;
}

const char *qbe_get_error(QBE *q) {
    return q ? q->error : "null context";
}

int qbe_target_count(void) {
    int count = 0;
    for (Target **t = tlist; *t; t++) count++;
    return count;
}

const char *qbe_target_name(int index) {
    int i = 0;
    for (Target **t = tlist; *t; t++, i++)
        if (i == index) return (*t)->name;
    return NULL;
}