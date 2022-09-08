#include "gen.h"

#include "../common/hash.h"
#include "../common/map.h"
#include "../common/error.h"
#include "../vm/vm.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

typedef struct data_s data_t;
typedef struct label_s label_t;
typedef struct replace_s replace_t;

struct data_s {
  int pos;
  hash_t str_hash;
  data_t *next;
};

struct label_s {
  hash_t name;
  int pos;
  label_t *next;
};

struct replace_s {
  int pos;
  replace_t *next;
};

static map_t map_data;
static data_t *data_list, *data_head;
static int data_size;
static int bss_size;

static instr_t *instr_buf;
static int num_instr, max_instr;
static int num_lbl;

static int func_active;
static hash_t ret_lbl;

static map_t map_replace;
static label_t *label_list;

int emit(instr_t instr);
void emit_jmp_hash(hash_t lbl);
void emit_label(instr_t instr, hash_t lbl);
void emit_frame_enter(int size);
void emit_frame_leave();
data_t *emit_data_str(hash_t str_hash);

void gen_func(func_t *func);
void gen_param(param_t *param);

void gen_stmt(stmt_t *stmt);
void gen_if(stmt_t *stmt);
void gen_while(stmt_t *stmt);
void gen_ret(stmt_t *stmt);
void gen_asm(stmt_t *stmt);
void gen_decl(stmt_t *stmt);

void gen_expr(expr_t *expr);
void gen_const(expr_t *expr);
void gen_addr(expr_t *expr);
void gen_call(expr_t *expr);
void gen_load(expr_t *expr);
void gen_cast(expr_t *expr);
void gen_str(expr_t *expr);

void gen_binop(expr_t *expr);
void gen_binop_assign(expr_t *expr);
void gen_binop_cond(expr_t *expr);
void gen_binop_math(expr_t *expr);

void gen_condition(expr_t *expr, hash_t end);

label_t *make_label(hash_t name, int pos);
replace_t *make_replace(int pos);
hash_t tmp_label();
void set_label(hash_t name);
void set_replace(hash_t name, int pos);
void replace_all();
tspec_t simplify_type_spec(type_t *type);
int sub_str_match_lhs(char *lhs, char *rhs);
void *collapse_data(int *data_len);

bin_t *gen(unit_t *unit)
{
  max_instr = 1024;
  num_lbl = 0;
  num_instr = 0;
  bss_size = unit->scope.size;
  
  instr_buf = malloc(max_instr * sizeof(instr_t));
  data_list = NULL;
  data_head = NULL;
  
  map_replace = make_map();
  map_data = make_map();
  
  gen_stmt(unit->stmt);
  emit(INT);
  emit(SYS_EXIT);
  
  gen_func(unit->func);
  
  replace_all();
  
  int data_size;
  void *data = collapse_data(&data_size);
  
  return make_bin(instr_buf, num_instr, data, data_size, (bss_size + 3) & (~3));
}

void gen_func(func_t *func)
{
  func_active = 1;
  
  while (func) {
    ret_lbl = tmp_label();
    
    set_label(func->name);
    
    emit_frame_enter(func->local_size);
    
    gen_param(func->params);
    gen_stmt(func->body);
    
    set_label(ret_lbl);
    emit_frame_leave();
    
    func = func->next;
  }
  
  func_active = 0;
}

void gen_param(param_t *param)
{
  if (!param)
    return;
  
  if (param->next)
    gen_param(param->next);
  
  gen_addr(param->addr);
  emit(STR);
}

void gen_stmt(stmt_t *stmt)
{
  while (stmt) {
    switch (stmt->tstmt) {
    case STMT_EXPR:
      gen_expr(stmt->expr);
      break;
    case STMT_IF:
      gen_if(stmt);
      break;
    case STMT_WHILE:
      gen_while(stmt);
      break;
    case STMT_RETURN:
      gen_ret(stmt);
      break;
    case STMT_INLINE_ASM:
      gen_asm(stmt);
      break;
    default:
      error("unknown case");
      break;
    }
    
    stmt = stmt->next;
  }
}

void gen_asm(stmt_t *stmt)
{
  char *c = stmt->inline_asm_stmt.code;
  
  while (*c) {
    switch (*c) {
    case ' ':
    case '\n':
      *c++;
      break;
    default:
      if (isdigit(*c) || *c == '-') {
        int signedness = 1;
        
        if (*c == '-') {
          signedness = -1;
          c++;
        }
        
        int sum = 0;
        while (isdigit(*c)) {
          sum = sum * 10 + *c - '0';
          c++;
        }
        
        emit(sum);
      } else {
        int match_keyword = 0;
        for (int i = 0; i < num_instr_tbl; i++) {
          if (sub_str_match_lhs(instr_tbl[i], c)) {
            match_keyword = 1;
            emit(i);
            c += strlen(instr_tbl[i]);
            break;
          }
        }
        
        if (!match_keyword)
          error("unknown character or keyword");
      }
      break;
    }
  }
}

void gen_ret(stmt_t *stmt)
{
  if (!func_active)
    error("ret_label while func inactive");
  
  gen_expr(stmt->ret_stmt.value);
  
  emit_label(JMP, ret_lbl);
}

void gen_if(stmt_t *stmt)
{
  if (!stmt)
    return;
  
  hash_t end_lbl = tmp_label();
  
  while (stmt) {
    hash_t cond_end_lbl = tmp_label();
    
    gen_condition(stmt->if_stmt.cond, cond_end_lbl);
    gen_stmt(stmt->if_stmt.body);
    emit_label(JMP, end_lbl);
    
    set_label(cond_end_lbl);
    
    if (stmt->if_stmt.else_body)
      gen_stmt(stmt->if_stmt.else_body);
    
    stmt = stmt->if_stmt.next_if;
  }
  
  set_label(end_lbl);
}

void gen_while(stmt_t *stmt)
{
  hash_t end_lbl = tmp_label();
  hash_t cond_lbl = tmp_label();
  
  set_label(cond_lbl);
  gen_condition(stmt->while_stmt.cond, end_lbl);
  gen_stmt(stmt->while_stmt.body);
  
  emit_label(JMP, cond_lbl);
  set_label(end_lbl);
}

void gen_expr(expr_t *expr)
{
  if (!expr)
    return;
  
  while (expr) {
    switch (expr->texpr) {
    case EXPR_CONST:
      gen_const(expr);
      break;
    case EXPR_ADDR:
      gen_addr(expr);
      break;
    case EXPR_LOAD:
      gen_load(expr);
      break;
    case EXPR_BINOP:
      gen_binop(expr);
      break;
    case EXPR_CALL:
      gen_call(expr);
      break;
    case EXPR_CAST:
      gen_cast(expr);
      break;
    case EXPR_STR:
      gen_str(expr);
      break;
    default:
      error("unknown case");
      break;
    }
    
    expr = expr->next;
  }
}

void gen_str(expr_t *expr)
{
  data_t *data = map_get(map_data, expr->str_hash);
  
  if (!data) {
    data = emit_data_str(expr->str_hash);
    map_put(map_data, expr->str_hash, data);
  }
  
  emit(PUSH);
  emit(data->pos);
}

void gen_cast(expr_t *expr)
{
  gen_expr(expr->unary.base);
  
  tspec_t type_a = simplify_type_spec(&expr->type);
  tspec_t type_b = simplify_type_spec(&expr->unary.base->type);
  
  switch (type_a) {
  case TY_I8:
    switch (type_b) {
    case TY_I8:
      break;
    case TY_I32:
      emit(SX32_8);
      break;
    case TY_STRUCT:
      break;
    }
    break;
  case TY_I32:
    switch (type_b) {
    case TY_I8:
      emit(SX8_32);
      break;
    case TY_I32:
      break;
    case TY_STRUCT:
      break;
    }
    break;
  case TY_STRUCT:
    break;
  }
}

void gen_call(expr_t *expr)
{
  func_t *func = expr->post.base->func.func;
  
  expr_t *arg = expr->post.post;
  while (arg) {
    gen_expr(arg->arg.base);
    arg = arg->arg.next;
  }
  
  emit(CALL);
  int pos = emit(0);
  
  set_replace(func->name, pos);
}

void gen_const(expr_t *expr)
{
  emit(PUSH);
  emit(expr->num);
}

void gen_addr(expr_t *expr)
{
  switch (expr->addr.taddr) {
  case ADDR_GLOBAL:
    gen_expr(expr->addr.base);
    break;
  case ADDR_LOCAL:
    emit(LBP);
    gen_expr(expr->addr.base);
    emit(ADD);
    break;
  default:
    error("unknown case");
    break;
  }
  
}

void gen_load(expr_t *expr)
{
  gen_addr(expr);
  tspec_t tspec = simplify_type_spec(&expr->type);
  switch (tspec) {
  case TY_I8:
    emit(LDR8);
    break;
  case TY_I32:
    emit(LDR);
    break;
  default:
    error("assign: unknown operator");
    break;
  }
}

void gen_condition(expr_t *expr, hash_t end)
{
  hash_t next_cond, yes_cond;
  instr_t *pos;
  
  switch (expr->texpr) {
  case EXPR_BINOP:
    switch (expr->binop.op) {
    case OPERATOR_AND:
      gen_condition(expr->binop.lhs, end);
      gen_condition(expr->binop.rhs, end);
      break;
    case OPERATOR_OR:
      next_cond = tmp_label();
      yes_cond = tmp_label();
      
      gen_condition(expr->binop.lhs, next_cond);
      
      emit_label(JMP, yes_cond);
      
      set_label(next_cond);
      gen_condition(expr->binop.rhs, end);
      
      set_label(yes_cond);
      
      break;
    case OPERATOR_EQ:
    case OPERATOR_NE:
    case OPERATOR_LE:
    case OPERATOR_GE:
    case OPERATOR_LSS:
    case OPERATOR_GTR:
      gen_expr(expr->binop.lhs);
      gen_expr(expr->binop.rhs);
      emit(CMP);
      
      switch (expr->binop.op) {
      case OPERATOR_EQ:
        emit_label(JNE, end);
        break;
      case OPERATOR_NE:
        emit_label(JE, end);
        break;
      case OPERATOR_LE:
        emit_label(JG, end);
        break;
      case OPERATOR_GE:
        emit_label(JL, end);
        break;
      case OPERATOR_LSS:
        emit_label(JGE, end);
        break;
      case OPERATOR_GTR:
        emit_label(JLE, end);
        break;
      }
      
      break;
    default:
      goto expr_cond;
    }
    break;
  expr_cond:
  default:
    gen_expr(expr);
    emit(PUSH);
    emit(0);
    emit(CMP);
    emit_label(JE, end);
    break;
  }
}

void gen_binop(expr_t *expr)
{
  switch (expr->binop.op) {
  case OPERATOR_ASSIGN:
    gen_binop_assign(expr);
    break;
  case OPERATOR_OR:
  case OPERATOR_AND:
    gen_binop_cond(expr);
    break;
  default:
    gen_binop_math(expr);
    break;
  }
}

void gen_binop_assign(expr_t *expr)
{
  gen_expr(expr->binop.rhs);
  gen_addr(expr->binop.lhs);
  
  tspec_t tspec = simplify_type_spec(&expr->type);
  switch (tspec) {
  case TY_I8:
    emit(STR8);
    break;
  case TY_I32:
    emit(STR);
    break;
  default:
    error("assign: unknown operator");
    break;
  }
}

void gen_binop_cond(expr_t *expr)
{
  hash_t cond_end = tmp_label();
  hash_t body_end = tmp_label();
  
  gen_condition(expr, cond_end);
  
  emit(PUSH);
  emit(1);
  emit(JMP);
  int pos = emit(0);
  
  set_replace(body_end, pos);
  
  set_label(cond_end);
  
  emit(PUSH);
  emit(0);
  
  set_label(body_end);
}

void gen_binop_math(expr_t *expr)
{
  gen_expr(expr->binop.lhs);
  gen_expr(expr->binop.rhs);
  
  tspec_t tspec = simplify_type_spec(&expr->type);
  switch (tspec) {
  case TY_I32:
    switch (expr->binop.op) {
    case OPERATOR_ADD:
      emit(ADD);
      break;
    case OPERATOR_SUB:
      emit(SUB);
      break;
    case OPERATOR_MUL:
      emit(MUL);
      break;
    case OPERATOR_DIV:
      emit(DIV);
      break;
    case OPERATOR_MOD:
      emit(MOD);
      break;
    case OPERATOR_EQ:
      emit(CMP);
      emit(SETE);
      break;
    case OPERATOR_NE:
      emit(CMP);
      emit(SETNE);
      break;
    case OPERATOR_LSS:
      emit(CMP);
      emit(SETL);
      break;
    case OPERATOR_GTR:
      emit(CMP);
      emit(SETG);
      break;
    case OPERATOR_LE:
      emit(CMP);
      emit(SETLE);
      break;
    case OPERATOR_GE:
      emit(CMP);
      emit(SETGE);
      break;
    default:
      error("unknown case: op: '%i'", expr->binop.op);
      break;
    }
    break;
  default:
    error("unknown case: spec: '%i'", tspec);
    break;
  }
}

label_t *make_label(hash_t name, int pos)
{
  label_t *label = malloc(sizeof(label_t));
  label->name = name;
  label->pos = pos;
  label->next = NULL;
  return label;
}

replace_t *make_replace(int pos)
{
  replace_t *replace = malloc(sizeof(replace_t));
  replace->pos = pos;
  replace->next = NULL;
  return replace;
}

hash_t tmp_label()
{
  char name[32];
  
  sprintf(name, ":_%lu", num_lbl++);
  
  return hash_value(name);
}

void set_label(hash_t name)
{
  if (label_list) {
    label_t *head = label_list;
    while (head->next) {
      if (name == head->name)
        error("duplicate label '%s'", hash_get(name));
      
      head = head->next;
    }
    
    if (name == head->name)
      error("duplicate label '%s'", hash_get(name));
    
    head->next = make_label(name, num_instr);
  } else {
    label_list = make_label(name, num_instr);
  }
}

void set_replace(hash_t name, int pos)
{
  replace_t *replace = map_get(map_replace, name);
  
  if (replace) {
    replace_t *head = replace;
    
    while (head->next)
      head = head->next;
    
    head->next = make_replace(pos);
  } else {
    map_put(map_replace, name, make_replace(pos));
  }
}

void replace_all()
{
  label_t *label = label_list;
  while (label) {
    replace_t *replace = map_get(map_replace, label->name);
    
    while (replace) {
      instr_buf[replace->pos] = label->pos;
      
      replace = replace->next;
    }
    
    label  = label->next;
  }
}

tspec_t simplify_type_spec(type_t *type)
{
  if (type->dcltr) {
    switch (type->dcltr->type) {
    case DCLTR_POINTER:
      return TY_I32;
    }
  }
  
  return type->spec->tspec;
}

int sub_str_match_lhs(char *lhs, char *rhs)
{
  char *c = lhs;
  
  int i = 0;
  while (*c) {
    if (*c++ != rhs[i++])
      return 0;
  }
  
  return 1;
}

void *collapse_data(int *data_size)
{
  data_t *data = data_list;
  
  *data_size = 0;
  while (data) {
    *data_size += strlen(hash_get(data->str_hash)) + 1;
    data = data->next;
  }
  
  void *buf = malloc(*data_size);
  void *ptr = buf;
  
  data = data_list;
  while (data) {
    char *str = hash_get(data->str_hash);
    int len = strlen(str) + 1;
    memcpy(ptr, str, len);
    ptr += len;
    
    data_t *next = data->next;
    free(data);
    data = next;
  }
  
  return buf;
}

int emit(instr_t instr)
{
  if (num_instr >= max_instr) {
    max_instr += 1024;
    instr_buf = realloc(instr_buf, max_instr);
  }
  
  int cache_pos = num_instr;
  instr_buf[num_instr++] = instr;
  
  return cache_pos;
}

void emit_frame_enter(int size)
{
  emit(ENTER);
  emit((size + 3) & (~3));
}

void emit_frame_leave()
{
  emit(LEAVE);
  emit(RET);
}

void emit_label(instr_t instr, hash_t lbl)
{
  emit(instr);
  int pos = emit(0);
  
  set_replace(lbl, pos);
}

data_t *emit_data_str(hash_t str_hash)
{
  data_t *data = malloc(sizeof(data_t));
  data->pos = data_size + bss_size;
  data->str_hash = str_hash;
  data->next = NULL;
  
  if (data_list)
    data_head = data_head->next = data;
  else
    data_list = data_head = data;
  
  data_size += strlen(hash_get(str_hash)) + 1;
  
  return data;
}
