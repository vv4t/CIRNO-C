#ifndef P_LOCAL_H
#define P_LOCAL_H

#include "parse.h"
#include "../common/error.h"

extern spec_t *ty_u0;
extern spec_t *ty_i8;
extern spec_t *ty_i32;

extern map_t scope_func;
extern map_t scope_struct;

extern scope_t *scope_local;
extern scope_t *scope_global;

extern func_t *current_func;
extern scope_t *current_scope;


//
// expr.c
//
int constant_expression(int *num);
expr_t *primary();
expr_t *cast();
expr_t *binop(int level);
expr_t *arg_expr_list();
expr_t *expression();

expr_t *make_expr();
expr_t *make_const(int num);
expr_t *make_addr(expr_t *base, taddr_t taddr, type_t *type);
expr_t *make_load(expr_t *base, taddr_t taddr, type_t *type);
expr_t *make_cast_expr(type_t *type, expr_t *base);
expr_t *make_func_expr(func_t *func);
expr_t *make_call(expr_t *func, expr_t *arg);
expr_t *make_arg(expr_t *base);
expr_t *make_binop(expr_t *lhs, operator_t op, expr_t *rhs);
expr_t *make_string_literal(hash_t str_hash);

//
// stmt.c
//
stmt_t *statement();
stmt_t *compound_statement();
stmt_t *if_statement();
stmt_t *while_statement();
stmt_t *expression_statement();
stmt_t *return_statement();
stmt_t *inline_asm_statement();
stmt_t *declaration_statement();

stmt_t *make_expr_stmt(expr_t *expr);
stmt_t *make_decl_stmt(decl_t *decl);
stmt_t *make_while_stmt(expr_t *cond, stmt_t *body);
stmt_t *make_if_stmt(expr_t *cond, stmt_t *body, stmt_t *next_if, stmt_t *else_body);
stmt_t *make_ret_stmt(expr_t *value);
stmt_t *make_inline_asm_stmt(char *code);

//
// decl.c
//
void decl_init();
func_t *func_declaration();
param_t *func_params();
void func_type(type_t *type);
int struct_declaration();
decl_t *struct_member_declaration();
decl_t *declaration(scope_t *scope);
dcltr_t *direct_declarator(hash_t *name);
dcltr_t *abstract_declarator();
dcltr_t *postfix_declarator(dcltr_t *base);
dcltr_t *pointer();
spec_t *specifiers();
int type_name(type_t *type);
param_t *param_type_list();
param_t *param_declaration();
decl_t *insert_decl(scope_t *scope, spec_t *spec, dcltr_t *dcltr, expr_t *init, hash_t name, int align_32);
int is_type_match(type_t *lhs, type_t *rhs);
int type_size(spec_t *spec, dcltr_t *dcltr);
int type_align(spec_t *spec, dcltr_t *dcltr);
spec_t *spec_cache_find(tspec_t tspec, scope_t *struct_scope);

scope_t *make_scope(taddr_t taddr);
dcltr_t *make_dcltr();
dcltr_t *make_dcltr_pointer(dcltr_t *next);
dcltr_t *make_dcltr_pointer(dcltr_t *next);
dcltr_t *make_dcltr_array(int size, dcltr_t *next);
decl_t *make_decl(spec_t *spec, dcltr_t *dcltr, expr_t *init, int offset);
param_t *make_param(spec_t *spec, dcltr_t *dcltr, expr_t *addr);
func_t *make_func(hash_t name, type_t *type, param_t *params, stmt_t *body, int local_size);

#endif
