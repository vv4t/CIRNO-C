#ifndef LEX_H
#define LEX_H

#include "../common/hash.h"
#include <stdio.h>

#define MAX_TMP 32
#define MAX_FSTACK 32
#define MIN_AVAILABLE 32

typedef struct lex_s lex_t;
typedef struct file_s file_t;
typedef enum token_e token_t;

enum token_e {
  TK_CONSTANT = 128,
  TK_STRING_LITERAL,
  TK_IDENTIFIER,
  TK_RIGHT_ASSIGN,
  TK_LEFT_ASSIGN,
  TK_ADD_ASSIGN,
  TK_SUB_ASSIGN,
  TK_MUL_ASSIGN,
  TK_DIV_ASSIGN,
  TK_MOD_ASSIGN,
  TK_AND_ASSIGN,
  TK_XOR_ASSIGN, 
  TK_OR_ASSIGN, 
  TK_RIGHT_OP,  
  TK_LEFT_OP,
  TK_INC_OP,
  TK_DEC_OP,
  TK_PTR_OP,
  TK_AND_OP,
  TK_OR_OP,
  TK_LE_OP,
  TK_GE_OP,
  TK_EQ_OP,
  TK_NE_OP,
  TK_ELLIPSIS,
  TK_FN,
  TK_I8,
  TK_I32,
  TK_IF,
  TK_WHILE,
  TK_RETURN,
  TK_BREAK,
  TK_ELSE,
  TK_STRUCT,
  TK_ASM,
  TK_ARGC,
  TK_ARGV
};

struct file_s {
  FILE *file;
  char *fname;
  int line_no;
  char tmp_buf[MAX_TMP];
  char *c;
};

struct lex_s {
  file_t fstack[MAX_FSTACK];
  file_t *fid;
  
  token_t token;
  int     token_num;
  hash_t  token_hash;
};

extern lex_t lex;

void lex_init();
void lexify(FILE *file, char *fname);
void next();
void match(token_t tok);
void token_error(const char *fmt, ...);
void token_warning(const char *fmt, ...);

#endif
