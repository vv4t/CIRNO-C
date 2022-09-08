#include "p_local.h"

#include <stdlib.h>

#define MAX_OP 8

typedef enum order_e order_t;
typedef struct opset_s opset_t;

enum order_e {
  ORDER_ASSIGNMENT,
  ORDER_OR,
  ORDER_AND,
  ORDER_EQUALATIVE,
  ORDER_ADDITIVE,
  ORDER_MULTIPLICATIVE
};

struct opset_s {
  token_t op[MAX_OP];
};

opset_t opset_dict[] = {
  { '=', TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN },
  { TK_OR_OP },
  { TK_AND_OP },
  { TK_EQ_OP, TK_NE_OP },
  { '<', '>', TK_LE_OP, TK_GE_OP },
  { '+', '-' },
  { '*', '/', '%' }
};

int num_opset_dict = sizeof(opset_dict) / sizeof(opset_t);

int read_assign_op(operator_t *op)
{
  switch (lex.token) {
  case '=':
    *op = OPERATOR_ASSIGN;
    break;
  case TK_ADD_ASSIGN:
    *op = OPERATOR_ADD;
    break;
  case TK_SUB_ASSIGN:
    *op = OPERATOR_SUB;
    break;
  case TK_MUL_ASSIGN:
    *op = OPERATOR_MUL;
    break;
  case TK_DIV_ASSIGN:
    *op = OPERATOR_DIV;
    break;
  case TK_MOD_ASSIGN:
    *op = OPERATOR_MOD;
    break;
  default:
    return 0;
  }
  
  next();
  return 1;
}

int read_expr_op(operator_t *op, int level)
{
  for (int i = 0; i < MAX_OP; i++) {
    if (opset_dict[level].op[i] == lex.token) {
      switch (lex.token) {
      case '+':
        *op = OPERATOR_ADD;
        break;
      case '-':
        *op = OPERATOR_SUB;
        break;
      case '*':
        *op = OPERATOR_MUL;
        break;
      case '/':
        *op = OPERATOR_DIV;
        break;
      case '%':
        *op = OPERATOR_MOD;
        break;
      case TK_OR_OP:
        *op = OPERATOR_OR;
        break;
      case TK_AND_OP:
        *op = OPERATOR_AND;
        break;
      case TK_EQ_OP:
        *op = OPERATOR_EQ;
        break;
      case TK_NE_OP:
        *op = OPERATOR_NE;
        break;
      case '>':
        *op = OPERATOR_GTR;
        break;
      case '<':
        *op = OPERATOR_LSS;
        break;
      case TK_LE_OP:
        *op = OPERATOR_LE;
        break;
      case TK_GE_OP:
        *op = OPERATOR_GE;
        break;
      default:
        return 0;
      }
      
      next();
      return 1;
    }
  }
  
  return 0;
}

int is_func(expr_t *expr)
{
  return expr->texpr == EXPR_FUNC;
}

int is_lvalue(expr_t *expr)
{
  return expr->texpr == EXPR_LOAD;
}

int is_array(expr_t *expr)
{
  return expr->type.dcltr && expr->type.dcltr->type == DCLTR_ARRAY;
}

int is_pointer(expr_t *expr)
{
  return expr->type.dcltr && expr->type.dcltr->type == DCLTR_POINTER;
}

int is_struct(expr_t *expr)
{
  return expr->type.spec->tspec == TY_STRUCT;
}

int is_arg_match(expr_t *func, expr_t *args)
{
  expr_t *arg = args;
  param_t *param = func->func.func->params;
  
  while (param) {
    if (!arg || !param)
      return 0;
    
    if (!is_type_match(&arg->arg.base->type, &param->type))
      return 0;
    
    arg = arg->arg.next;
    param = param->next;
  }
  
  return 1;
}

expr_t *find_identifier()
{
  hash_t name = lex.token_hash;
  
  decl_t *decl;
  func_t *func;
  if ((decl = map_get(scope_local->map, name))) {
    match(TK_IDENTIFIER);
    return make_load(make_const(decl->offset), ADDR_LOCAL, &decl->type);
  } else if ((decl = map_get(scope_global->map, name))) {
    match(TK_IDENTIFIER);
    return make_load(make_const(decl->offset), ADDR_GLOBAL, &decl->type);
  } else if ((func = map_get(scope_func, name))) {
    match(TK_IDENTIFIER);
    return make_func_expr(func);
  } else {
    token_error("'%n' undeclared");
    return NULL;
  }
}

expr_t *primary()
{
  expr_t *expr = NULL;
  switch (lex.token) {
  case TK_CONSTANT:
    expr = make_const(lex.token_num);
    match(TK_CONSTANT);
    break;
  case TK_STRING_LITERAL:
    expr = make_string_literal(lex.token_hash);
    match(TK_STRING_LITERAL);
    break;
  case TK_IDENTIFIER:
    expr = find_identifier();
    break;
  case '(':
    match('(');
    expr = expression();
    match(')');
    break;
  default:
    return NULL;
  }
  
  return expr;
}

expr_t *postfix()
{
  expr_t *expr = primary();
  
  if (!expr)
    return NULL;
  
  while (1) {
    if (lex.token == '[') {
      match('[');
      expr_t *post = expression();
      match(']');
      
      if (!is_array(expr) && !is_pointer(expr))
        token_error("cannot index non-array");
      
      type_t array_type = { expr->type.spec, expr->type.dcltr->next };
      
      expr_t *align = make_const(type_size(array_type.spec, array_type.dcltr));
      expr_t *offset = make_binop(post, OPERATOR_MUL, align);
      
      if (is_array(expr)) {
        expr_t *base = make_binop(expr->addr.base, OPERATOR_ADD, offset);
        expr = make_load(base, expr->addr.taddr, &array_type);
      } else if (is_pointer(expr)) {
        expr_t *base = make_binop(expr, OPERATOR_ADD, offset);
        expr = make_load(base, ADDR_GLOBAL, &array_type);
      }
    } else if (lex.token == '(') {
      match('(');
      expr_t *post = arg_expr_list();
      match(')');
      
      if (!is_func(expr))
        token_error("cannot call non-function");
      
      if (!is_arg_match(expr, post))
        token_error("incorrect arguments");
      
      return make_call(expr, post);
    } else if (lex.token == '.') {
      match('.');
      
      hash_t name = lex.token_hash;
      match(TK_IDENTIFIER);
      
      if (!is_struct(expr))
        token_error("cannot use '.' operator on non-struct");
      
      if (is_pointer(expr))
        token_error("cannot use '.' operator on struct-pointer; did you mean '->'?");
      
      decl_t *struct_decl = map_get(expr->type.spec->struct_scope->map, name);
      
      expr_t *base = make_binop(expr->addr.base, OPERATOR_ADD, make_const(struct_decl->offset));
      expr = make_load(base, expr->addr.taddr, &struct_decl->type);
    } else if (lex.token == TK_PTR_OP) {
      match(TK_PTR_OP);
      
      hash_t name = lex.token_hash;
      match(TK_IDENTIFIER);
      
      if (!is_struct(expr))
        token_error("cannot use '->' operator on non-struct");
      
      if (!is_pointer(expr))
        token_error("cannot use '->' operator on non-struct-pointer; did you mean '.'?");
      
      decl_t *struct_decl = map_get(expr->type.spec->struct_scope->map, name);
      
      expr_t *base = make_binop(expr, OPERATOR_ADD, make_const(struct_decl->offset));
      expr = make_load(base, ADDR_GLOBAL, &struct_decl->type);
    } else {
      break;
    }
  }
  
  return expr;
}

expr_t *arg_expr_list()
{
  expr_t *args, *head, *base;
  
  base = binop(ORDER_ASSIGNMENT);
  if (!base)
    return NULL;
  
  head = args = make_arg(base);
  
  while (lex.token == ',') {
    match(',');
    
    base = binop(ORDER_ASSIGNMENT);
    if (!head)
      token_error("expected expression");
    
    head = head->arg.next = make_arg(base);
  }
  
  return args;
}

expr_t *unary()
{
  if (lex.token == '&') {
    match('&');
    
    expr_t *expr = cast();
    
    if (!is_lvalue(expr))
      token_error("unary operator '&' requires lvalue");
    
    expr->texpr = EXPR_ADDR;
    expr->type.dcltr = make_dcltr_pointer(expr->type.dcltr);
    
    return expr;
  } else if(lex.token == '*') {
    match('*');
    
    expr_t *expr = cast();
    
    if (!is_pointer(expr))
      token_error("cannot cast indirection on non-pointer");
    
    type_t indirect_type = { expr->type.spec, expr->type.dcltr->next };
    
    return make_load(expr, ADDR_GLOBAL, &indirect_type);
  } if (lex.token == '-') {
    match('-');
    return make_binop(cast(), OPERATOR_MUL, make_const(-1));
  } else if (lex.token == '+') {
    match('+');
    return cast();
  } else {
    return postfix();
  }
}

expr_t *cast()
{
  if (lex.token == '(') {
    match('(');
    
    type_t type = { 0 };
    if (type_name(&type)) {
      match(')');
      return make_cast_expr(&type, unary());
    } else {
      expr_t *base = expression();
      match(')');
      return base;
    }
  }
  
  return unary();
}

expr_t *binop(int level)
{
  if (level >= num_opset_dict)
    return cast();
  
  expr_t *lhs = binop(level + 1);
  
  if (!lhs)
    return NULL;
  
  operator_t op;
  if (level == ORDER_ASSIGNMENT) {
   if (read_assign_op(&op)) {
      expr_t *rhs = binop(level + 1);
      
      if (!rhs)
        token_error("expected expression");
      
      if (!is_lvalue(lhs))
        token_error("cannot assign non-lvalue");
      
      if (!is_type_match(&lhs->type, &rhs->type))
        token_error("type mismatch");
      
      if (op == OPERATOR_ASSIGN)
        return make_binop(lhs, op, rhs);
      else
        return make_binop(lhs, OPERATOR_ASSIGN, make_binop(lhs, op, rhs));
    }
  } else {
    while (read_expr_op(&op, level))
      lhs = make_binop(lhs, op, binop(level + 1));
  }
  
  return lhs;
}

expr_t *expression()
{
  expr_t *head = NULL, *body = NULL;
  
  body = head = binop(ORDER_ASSIGNMENT);
  if (!body)
    return NULL;
  
  while (lex.token == ',') {
    match(',');
    head = head->next = binop(ORDER_ASSIGNMENT);
  }
  
  return body;
}

int constant_expression(int *num)
{
  expr_t *expr = binop(0);
  
  if (expr->texpr != EXPR_CONST)
    return 0;
  
  *num = expr->num;
  
  return 1;
}

expr_t *make_expr()
{
  expr_t *expr = malloc(sizeof(expr_t));
  expr->next = NULL;
  expr->texpr = EXPR_NONE;
  return expr;
}

expr_t *make_const(int num)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_CONST;
  expr->num = num;
  expr->type.spec = ty_i32;
  expr->type.dcltr = NULL;
  return expr;
}

expr_t *make_addr(expr_t *base, taddr_t taddr, type_t *type)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_ADDR;
  expr->addr.base = base;
  expr->addr.taddr = taddr;
  expr->type.spec = type->spec;
  expr->type.dcltr = type->dcltr;
  return expr;
}

expr_t *make_load(expr_t *base, taddr_t taddr, type_t *type)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_LOAD;
  expr->addr.base = base;
  expr->addr.taddr = taddr;
  expr->type.spec = type->spec;
  expr->type.dcltr = type->dcltr;
  return expr;
}

expr_t *make_cast_expr(type_t *type, expr_t *base)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_CAST;
  expr->unary.base = base;
  expr->type.spec = type->spec;
  expr->type.dcltr = type->dcltr;
  return expr;
}

expr_t *make_func_expr(func_t *func)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_FUNC;
  expr->func.func = func;
  expr->type.spec = spec_cache_find(TY_FUNC, NULL);
  expr->type.dcltr = NULL;
  return expr;
}

expr_t *make_call(expr_t *func, expr_t *arg)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_CALL;
  expr->post.base = func;
  expr->post.post = arg;
  expr->type.spec = func->func.func->type.spec;
  expr->type.dcltr = func->func.func->type.dcltr;
  return expr;
}

expr_t *make_arg(expr_t *base)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_ARG;
  expr->arg.base = base;
  expr->arg.next = NULL;
  return expr;
}

expr_t *make_binop(expr_t *lhs, operator_t op, expr_t *rhs)
{
  if (lhs->texpr == EXPR_CONST && rhs->texpr == EXPR_CONST) {
    switch (op) {
    case OPERATOR_ADD:
      return make_const(lhs->num + rhs->num);
    case OPERATOR_SUB:
      return make_const(lhs->num - rhs->num);
    case OPERATOR_MUL:
      return make_const(lhs->num * rhs->num);
    case OPERATOR_DIV:
      return make_const(lhs->num / rhs->num);
    case OPERATOR_MOD:
      return make_const(lhs->num % rhs->num);
    default:
      error("unknown operator");
      break;
    }
  }
  
  expr_t *expr = make_expr();
  expr->texpr = EXPR_BINOP;
  expr->binop.op = op;
  expr->binop.lhs = lhs;
  expr->binop.rhs = rhs;
  expr->type.spec = lhs->type.spec;
  expr->type.dcltr = lhs->type.dcltr;
  return expr;
}

expr_t *make_string_literal(hash_t str_hash)
{
  expr_t *expr = make_expr();
  expr->texpr = EXPR_STR;
  expr->str_hash = str_hash;
  expr->type.spec = ty_i8;
  expr->type.dcltr = make_dcltr_pointer(NULL);
  return expr;
}
