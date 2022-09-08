#include "vm.h"

#include "../common/error.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ALIGN_32(X) (X / 4)

vm_t *make_vm()
{
  vm_t *vm = malloc(sizeof(vm_t));
  vm->ip = 0;
  vm->sp = 0;
  vm->bp = MAX_MEM * sizeof(int);
  vm->cp = 0;
  vm->fp = 0;
  vm->f_gtr = 0;
  vm->f_lss = 0;
  vm->f_equ = 0;
  vm->f_exit = 0;
  vm->s_i32 = vm->stack;
  vm->m_i8 = (char*) vm->mem;
  vm->m_i32 = vm->mem;
  return vm;
}

instr_t fetch(vm_t *vm)
{
  return vm->bin->instr[vm->ip++];
}

int pop(vm_t *vm)
{
  return vm->s_i32[--vm->sp];
}

static inline void vm_push(vm_t *vm, int i32)
{
  vm->s_i32[vm->sp++] = i32;
}

static inline void vm_add(vm_t *vm)
{
  vm->s_i32[vm->sp - 2] += vm->s_i32[vm->sp - 1];
  --vm->sp;
}

static inline void vm_sub(vm_t *vm)
{
  vm->s_i32[vm->sp - 2] -= vm->s_i32[vm->sp - 1];
  --vm->sp;
}

static inline void vm_mul(vm_t *vm)
{
  vm->s_i32[vm->sp - 2] *= vm->s_i32[vm->sp - 1];
  --vm->sp;
}

static inline void vm_div(vm_t *vm)
{
  vm->s_i32[vm->sp - 2] /= vm->s_i32[vm->sp - 1];
  --vm->sp;
}

static inline void vm_ldr(vm_t *vm)
{
  vm->s_i32[vm->sp - 1] = vm->m_i32[ALIGN_32(vm->s_i32[vm->sp - 1])];
}

static inline void vm_ldr8(vm_t *vm)
{
  vm->s_i32[vm->sp - 1] = vm->m_i8[vm->s_i32[vm->sp - 1]];
}

static inline void vm_str(vm_t *vm)
{
  vm->m_i32[ALIGN_32(vm->s_i32[vm->sp - 1])] = vm->s_i32[vm->sp - 2];
  vm->sp -= 2;
}

static inline void vm_str8(vm_t *vm)
{
  vm->m_i8[vm->s_i32[vm->sp - 1]] = vm->s_i32[vm->sp - 2];
  vm->sp -= 2;
}

static inline void vm_lbp(vm_t *vm)
{
  vm->s_i32[vm->sp++] = vm->bp;
}

static inline void vm_enter(vm_t *vm, int i32)
{
  vm->frame[vm->fp++] = vm->bp;
  vm->bp -= i32;
}

static inline void vm_leave(vm_t *vm)
{
  vm->bp = vm->frame[--vm->fp];
}

static inline void vm_call(vm_t *vm, int i32)
{
  vm->call[vm->cp++] = vm->ip;
  vm->ip = i32;
}

static inline void vm_ret(vm_t *vm)
{
  vm->ip = vm->call[--vm->cp];
}

static inline void vm_jmp(vm_t *vm, int i32)
{
  vm->ip = i32;
}

static inline void vm_cmp(vm_t *vm)
{
  int tmp = vm->s_i32[vm->sp - 2] - vm->s_i32[vm->sp - 1];
  vm->sp -= 2;
  
  vm->f_gtr = tmp > 0;
  vm->f_lss = tmp < 0;
  vm->f_equ = tmp == 0;
}

static inline void vm_je(vm_t *vm, int i32)
{
  if (vm->f_equ)
    vm->ip = i32;
}

static inline void vm_jne(vm_t *vm, int i32)
{
  if (!vm->f_equ)
    vm->ip = i32;
}

static inline void vm_jl(vm_t *vm, int i32)
{
  if (vm->f_lss)
    vm->ip = i32;
}

static inline void vm_jg(vm_t *vm, int i32)
{
  if (vm->f_gtr)
    vm->ip = i32;
}

static inline void vm_jle(vm_t *vm, int i32)
{
  if (vm->f_equ || vm->f_lss)
    vm->ip = i32;
}

static inline void vm_jge(vm_t *vm, int i32)
{
  if (vm->f_equ || vm->f_gtr)
    vm->ip = i32;
}

static inline void vm_sete(vm_t *vm)
{
  vm_push(vm, vm->f_equ);
}

static inline void vm_setne(vm_t *vm)
{
  vm_push(vm, !vm->f_equ);
}

static inline void vm_setl(vm_t *vm)
{
  vm_push(vm, vm->f_lss);
}

static inline void vm_setg(vm_t *vm)
{
  vm_push(vm, vm->f_gtr);
}

static inline void vm_setle(vm_t *vm)
{
  vm_push(vm, vm->f_equ || vm->f_lss);
}

static inline void vm_setge(vm_t *vm)
{
  vm_push(vm, vm->f_equ || vm->f_gtr);
}

static inline void vm_sx8_32(vm_t *vm)
{
  int is_sign = (vm->s_i32[vm->sp - 1] & 0x80);
  vm->s_i32[vm->sp - 1] = (is_sign << 24) | (is_sign ? (vm->s_i32[vm->sp - 1] | ~0x7f) : (vm->s_i32[vm->sp - 1] & 0x7f));
}

static inline void vm_sx32_8(vm_t *vm)
{
  vm->s_i32[vm->sp - 1] = ((vm->s_i32[vm->sp - 1] & 0x80000000) >> 24) | (vm->s_i32[vm->sp - 1] & 0x7f);
}

static inline void vm_mod(vm_t *vm)
{
  vm->s_i32[vm->sp - 2] %= vm->s_i32[vm->sp - 1];
  --vm->sp;
}

static inline void vm_exit(vm_t *vm)
{
  vm->f_exit = 1;
}

static inline void vm_print(vm_t *vm)
{
  printf("%i\n", vm->s_i32[vm->sp - 1]);
  vm->sp -= 1;
}

static inline void vm_write(vm_t *vm)
{
  fputs(&vm->m_i8[vm->s_i32[vm->sp - 1]], stdout);
  vm->sp -= 1;
}

static inline void vm_int(vm_t *vm, int code)
{
  switch (code) {
  case SYS_EXIT:
    vm_exit(vm);
    break;
  case SYS_PRINT:
    vm_print(vm);
    break;
  case SYS_WRITE:
    vm_write(vm);
    break;
  }
}

void vm_load(vm_t *vm, bin_t *bin)
{
  vm->bin = bin;
  vm->ip = 0;
  vm->bp = MAX_MEM * sizeof(int);
  vm->sp = 0;
  vm->f_exit = 0;
  
  memcpy(vm->m_i8 + bin->bss_size, bin->data, bin->data_size);
}

void vm_exec(vm_t *vm)
{
  while (!vm->f_exit) {
    switch (fetch(vm)) {
    case PUSH:
      vm_push(vm, fetch(vm));
      break;
    case ENTER:
      vm_enter(vm, fetch(vm));
      break;
    case ADD:
      vm_add(vm);
      break;
    case SUB:
      vm_sub(vm);
      break;
    case MUL:
      vm_mul(vm);
      break;
    case DIV:
      vm_div(vm);
      break;
    case MOD:
      vm_mod(vm);
      break;
    case LDR:
      vm_ldr(vm);
      break;
    case LDR8:
      vm_ldr8(vm);
      break;
    case STR:
      vm_str(vm);
      break;
    case STR8:
      vm_str8(vm);
      break;
    case LBP:
      vm_lbp(vm);
      break;
    case CALL:
      vm_call(vm, fetch(vm));
      break;
    case LEAVE:
      vm_leave(vm);
      break;
    case RET:
      vm_ret(vm);
      break;
    case JMP:
      vm_jmp(vm, fetch(vm));
      break;
    case CMP:
      vm_cmp(vm);
      break;
    case JE:
      vm_je(vm, fetch(vm));
      break;
    case JNE:
      vm_jne(vm, fetch(vm));
      break;
    case JL:
      vm_jl(vm, fetch(vm));
      break;
    case JG:
      vm_jg(vm, fetch(vm));
      break;
    case JLE:
      vm_jle(vm, fetch(vm));
      break;
    case JGE:
      vm_jge(vm, fetch(vm));
      break;
    case SETE:
      vm_sete(vm);
      break;
    case SETNE:
      vm_setne(vm);
      break;
    case SETL:
      vm_setl(vm);
      break;
    case SETG:
      vm_setg(vm);
      break;
    case SETLE:
      vm_setle(vm);
      break;
    case SETGE:
      vm_setge(vm);
      break;
    case SX8_32:
      vm_sx8_32(vm);
      break;
    case SX32_8:
      vm_sx32_8(vm);
      break;
    case INT:
      vm_int(vm, fetch(vm));
      break;
    default:
      error("unknown op");
      break;
    }
  }
}
