#ifndef ERROR_H
#define ERROR_H

#include <stdlib.h>

#define error(...) { fprintf(stderr, "%s:%i:%s: ", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(-1); }

#endif
