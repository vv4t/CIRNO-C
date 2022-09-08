#ifndef VM_H
#define VM_H

#define KB(B) (B * 1024)

#define MAX_STACK 128
#define MAX_CALL 64
#define MAX_MEM 64
#define MAX_CALL 64
#define MAX_FRAME 64

#include "bin.h"
#include "instr.h"
#include "../common/hash.h"

typedef struct vm_s vm_t;
typedef struct call_s call_t;
typedef enum int_code_e int_code_t;

enum int_code_e {
  SYS_EXIT,
  SYS_PRINT,
  SYS_WRITE
};

struct vm_s {
  bin_t *bin;
  int ip, sp, bp, cp, fp;
  int f_gtr, f_lss, f_equ, f_exit;
  int mem[MAX_MEM];
  int stack[MAX_STACK];
  int call[MAX_CALL];
  int frame[MAX_FRAME];
  int *s_i32;
  char *m_i8;
  int *m_i32;
};

vm_t *make_vm();
void vm_load(vm_t *vm, bin_t *bin);
void vm_exec(vm_t *vm);

#endif
