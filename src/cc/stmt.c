#include "p_local.h"

#include <stdlib.h>

stmt_t *make_stmt()
{
  return malloc(sizeof(stmt_t));
}

stmt_t *statement()
{
  stmt_t *stmt = NULL;
  if ((stmt = if_statement())
  || (stmt = while_statement())
  || (stmt = compound_statement())
  || (stmt = return_statement())
  || (stmt = inline_asm_statement())
  || (stmt = declaration_statement())
  || (stmt = expression_statement()))
    return stmt;
  
  return NULL;
}

stmt_t *inline_asm_statement()
{
  if (lex.token != TK_ASM)
    return NULL;
  
  match(TK_ASM);
  
  match('(');
  hash_t code_hash = lex.token_hash;
  match(TK_STRING_LITERAL);
  match(')');
  match(';');
  
  char *code = hash_get(code_hash);
  
  return make_inline_asm_stmt(code);
}

stmt_t *compound_statement()
{
  if (lex.token != '{')
    return NULL;
  
  match('{');
  
  stmt_t *body = NULL, *head = NULL, *stmt = NULL;
  while (lex.token != '}') {
    while (!(stmt = statement()));
    if (body)
      head = head->next = stmt;
    else
      body = head = stmt;
  }
  
  match('}');
  
  return body;
}

stmt_t *return_statement()
{
  if (lex.token != TK_RETURN)
    return NULL;
  
  match(TK_RETURN);
  
  expr_t *value = NULL;
  
  if (lex.token != ';')
    value = expression();
  
  match(';');
  
  if (current_func->type.spec) {
    if (!value)
      token_error("expected return value");
    
    if (!is_type_match(&current_func->type, &value->type))
      token_error("type mismatch");
  } else {
    if (value)
      token_error("cannot return value in non-return function");
  }
  
  return make_ret_stmt(value);
}

stmt_t *if_statement()
{
  if (lex.token != TK_IF)
    return NULL;
  
  match(TK_IF);
  
  match('(');
  expr_t *cond = expression();
  match(')');
  
  if (!cond)
    token_error("expected expression");
  
  stmt_t *body = statement();
  
  if (lex.token == TK_ELSE) {
    match(TK_ELSE);
    
    if (lex.token == TK_IF)
      return make_if_stmt(cond, body, if_statement(), NULL);
    else
      return make_if_stmt(cond, body, NULL, statement());
  }
  
  return make_if_stmt(cond, body, NULL, NULL);
}

stmt_t *while_statement()
{
  if (lex.token != TK_WHILE)
    return NULL;
  
  match(TK_WHILE);
  
  match('(');
  expr_t *cond = expression();
  match(')');
  
  if (!cond)
    token_error("expected expression");
  
  stmt_t *body = statement();
  
  return make_while_stmt(cond, body);
}

stmt_t *declaration_statement()
{
  decl_t *decl = declaration(current_scope);
  if (!decl)
    return NULL;
  
  expr_t *body = NULL, *head = NULL;
  while (decl) {
    if (decl->init) {
      expr_t *addr = make_load(make_const(decl->offset), current_scope->taddr, &decl->type);
      expr_t *init = make_binop(addr, OPERATOR_ASSIGN, decl->init);
      
      if (body)
        head = head->next = init;
      else
        body = head = init;
    }
    
    decl = decl->next;
  }
  
  return make_expr_stmt(body);
}

stmt_t *expression_statement()
{
  expr_t *expr = expression();
  if (!expr)
    return NULL;
  
  match(';');
  
  return make_expr_stmt(expr);
}

stmt_t *make_expr_stmt(expr_t *expr)
{
  stmt_t *stmt = make_stmt();
  stmt->tstmt = STMT_EXPR;
  stmt->expr = expr;
  stmt->next = NULL;
  return stmt;
}

stmt_t *make_while_stmt(expr_t *cond, stmt_t *body)
{
  stmt_t *stmt = make_stmt();
  stmt->tstmt = STMT_WHILE;
  stmt->while_stmt.cond = cond;
  stmt->while_stmt.body = body;
  stmt->next = NULL;
  return stmt;
}

stmt_t *make_if_stmt(expr_t *cond, stmt_t *body, stmt_t *next_if, stmt_t *else_body)
{
  stmt_t *stmt = make_stmt();
  stmt->tstmt = STMT_IF;
  stmt->if_stmt.cond = cond;
  stmt->if_stmt.body = body;
  stmt->if_stmt.next_if = next_if;
  stmt->if_stmt.else_body = else_body;
  stmt->next = NULL;
  return stmt;
}

stmt_t *make_ret_stmt(expr_t *value)
{
  stmt_t *stmt = make_stmt();
  stmt->tstmt = STMT_RETURN;
  stmt->ret_stmt.value = value;
  stmt->next = NULL;
  return stmt;
}

stmt_t *make_inline_asm_stmt(char *code)
{
  stmt_t *stmt = make_stmt();
  stmt->tstmt = STMT_INLINE_ASM;
  stmt->inline_asm_stmt.code = code;
  stmt->next = NULL;
  return stmt;
}
