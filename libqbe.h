#ifndef LIBQBE_H
#define LIBQBE_H

#include <stdio.h>

typedef struct QBE QBE;

QBE *qbe_new(void);
void qbe_free(QBE *q);

int qbe_init_defaults(QBE *q);

int  qbe_set_target(QBE *q, const char *target_name);
int  qbe_set_output_file(QBE *q, const char *filename);
void qbe_set_debug_flags(QBE *q, const char *flags);

int  qbe_compile_string(QBE *q, const char *input, const char *name);
int  qbe_compile_file(QBE *q, const char *filename);
int  qbe_compile_stream(QBE *q, FILE *stream, const char *name);

const char *qbe_get_error(QBE *q);

int   qbe_target_count(void);
const char *qbe_target_name(int index);

int qbe_finish(QBE *q);

#endif
