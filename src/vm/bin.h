#ifndef BIN_H
#define BIN_H

#include "instr.h"
#include "../common/hash.h"
#include <stdio.h>

typedef struct bin_s bin_t;
typedef struct sym_s sym_t;

struct bin_s {
  instr_t *instr;
  int num_instr;
  void *data;
  int data_size;
  int bss_size;
};

struct sym_s {
  hash_t name;
  int pos;
};

extern char *instr_tbl[];
extern int num_instr_tbl;

void bin_dump(bin_t *bin);
void bin_write(bin_t *bin, FILE *out);
bin_t *bin_read(FILE *in);

bin_t *make_bin(instr_t *instr, int num_instr, void *data, int data_size, int bss_size);

#endif
