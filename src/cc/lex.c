#include "lex.h"

#include "../common/error.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define MAX_OP 4
#define MAX_WORD 32

typedef struct op_s op_t;
typedef struct keyword_s keyword_t;

struct op_s {
  const char key[MAX_OP];
  token_t token;
};

struct keyword_s {
  const char key[MAX_WORD];
  token_t token;
};

const char *token_str[] = {
  "constant",
  "string literal",
  "identifier",
  ">>=",
  "<<=",
  "+=",
  "-=",
  "*=",
  "/=",
  "%=",
  "&=",
  "^=", 
  "|=", 
  ">>",  
  "<<",
  "++",
  "--",
  "->",
  "&&",
  "||",
  "<=",
  ">=",
  "==",
  "!=",
  "...",
  "fn",
  "i8",
  "i32",
  "if",
  "while",
  "return",
  "break",
  "else",
  "asm"
};

op_t op_dict[] = {
  { "...",  TK_ELLIPSIS         },
  { ">>=",  TK_RIGHT_ASSIGN     }, 
  { "<<=",  TK_LEFT_ASSIGN      },
  { "+=",   TK_ADD_ASSIGN       },
  { "-=",   TK_SUB_ASSIGN       },
  { "*=",   TK_MUL_ASSIGN       },
  { "/=",   TK_DIV_ASSIGN       },
  { "%=",   TK_MOD_ASSIGN       },
  { "&=",   TK_AND_ASSIGN       },
  { "^=",   TK_XOR_ASSIGN       },
  { "|=",   TK_OR_ASSIGN        },
  { ">>",   TK_RIGHT_OP         },
  { "<<",   TK_LEFT_OP          },
  { "++",   TK_INC_OP           },
  { "--",   TK_DEC_OP           },
  { "->",   TK_PTR_OP           },
  { "&&",   TK_AND_OP           },
  { "||",   TK_OR_OP            },
  { "<=",   TK_LE_OP            },
  { ">=",   TK_GE_OP            },
  { "==",   TK_EQ_OP            },
  { "!=",   TK_NE_OP            },
  { ";",    ';'                 },
  { "{",    '{'                 },
  { "}",    '}'                 },
  { ",",    ','                 },
  { ":",    ':'                 },
  { "=",    '='                 },
  { "(",    '('                 },
  { ")",    ')'                 },
  { "[",    '['                 },
  { "]",    ']'                 },
  { ".",    '.'                 },
  { "&",    '&'                 },
  { "!",    '!'                 },
  { "~",    '~'                 },
  { "-",    '-'                 },
  { "+",    '+'                 },
  { "*",    '*'                 },
  { "/",    '/'                 },
  { "%",    '%'                 },
  { "<",    '<'                 },
  { ">",    '>'                 },
  { "^",    '^'                 },
  { "|",    '|'                 },
  { "?",    '?'                 },
  { ":",    ':'                 }
};

keyword_t keyword_dict[] = {
  { "fn",       TK_FN           },
  { "i8",       TK_I8           },
  { "i32",      TK_I32          },
  { "if",       TK_IF           },
  { "while",    TK_WHILE        },
  { "return",   TK_RETURN       },
  { "break",    TK_BREAK        },
  { "else",     TK_ELSE         },
  { "struct",   TK_STRUCT       },
  { "asm",      TK_ASM          },
  { "argc",     TK_ARGC         },
  { "argv",     TK_ARGV         }
};

const int op_dict_count = sizeof(op_dict) / sizeof(op_t);
const int keyword_dict_count = sizeof(keyword_dict) / sizeof(keyword_t);

lex_t lex;

void token_fprint(FILE *out, token_t token)
{
  if (token >= TK_CONSTANT)
    fprintf(out, "%s", token_str[token - TK_CONSTANT]);
  else
    fprintf(out, "%c", token);
}

void token_fprint_current(FILE *out)
{
  switch (lex.token) {
  case TK_CONSTANT:
    fprintf(out, "%i", lex.token_num);
    break;
  case TK_STRING_LITERAL:
    fprintf(out, "%s", hash_get(lex.token_hash));
    break;
  case TK_IDENTIFIER:
    fprintf(out, "%s", hash_get(lex.token_hash));
    break;
  default:
    token_fprint(out, lex.token);
    break;
  }
}

void token_fprintf(FILE *out, const char *fmt, va_list args)
{
  while (*fmt) {
    if (*fmt == '%') {
      switch (*++fmt) {
      case 'c':
        fprintf(out, "%c", va_arg(args, int));
        break;
      case 'i':
        fprintf(out, "%i", va_arg(args, int));
        break;
      case 's':
        fprintf(out, "%s", va_arg(args, char *));
        break;
      case 't':
        token_fprint(out, va_arg(args, token_t));
        break;
      case 'n':
        token_fprint_current(out);
        break;
      }
      
      *++fmt;
    } else {
      fprintf(out, "%c", *fmt++);
    }
  }
}

void token_warning(const char *fmt, ...)
{
  printf("%s:%i:warning: ", lex.fid->fname, lex.fid->line_no);
  
  va_list args;
  va_start(args, fmt);
  token_fprintf(stdout, fmt, args);
  va_end(args);
  
  printf("\n");
}

void token_error(const char *fmt, ...)
{
  fprintf(stderr, "%s:%i:error: ", lex.fid->fname, lex.fid->line_no);
  
  va_list args;
  va_start(args, fmt);
  token_fprintf(stderr, fmt, args);
  va_end(args);
  
  fprintf(stderr, "\n");
  exit(-1);
}

void populate_buffer()
{
  int bytes_old = lex.fid->c - lex.fid->tmp_buf;
  int available = MAX_TMP - bytes_old;
  
  if (available < MIN_AVAILABLE) {
    if (available)
      memmove(lex.fid->tmp_buf, lex.fid->c, available);
    
    for (int i = 0; i < bytes_old; i++)
      lex.fid->tmp_buf[available + i] = fgetc(lex.fid->file);
    
    lex.fid->c = lex.fid->tmp_buf;
  }
}

int read_char()
{
  int c = *lex.fid->c++;
  populate_buffer();
  
  return c;
}

int count(int n)
{
  if (*lex.fid->c == '\n')
    lex.fid->line_no++;
  
  lex.fid->c += n;
  populate_buffer();
}

void match(token_t token)
{
  if (lex.token == token)
    next();
  else
    token_error("unexpected token '%n', expected token '%t'", token);
}

int is_digit(int c)
{
  return c >= '0' && c <= '9';
}

int to_digit(int c)
{
  if (!is_digit(c))
    error("cannot convert '%c' into digit.", c);
  
  return c - '0';
}

int is_letter(int c)
{
  return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

int read_escape_sequence()
{
  if (*lex.fid->c != '\\')
    token_error("expected '\\'");
  
  count(1);
  
  int num = 0;
  switch (*lex.fid->c) {
  case 'a':
    num = '\a';
    count(1);
    break;
  case 'b':
    num = '\b';
    count(1);
    break;
  case 'f':
    num = '\f';
    count(1);
    break;
  case 'n':
    num = '\n';
    count(1);
    break;
  case 'r':
    num = '\r';
    count(1);
    break;
  case 't':
    num = '\t';
    count(1);
    break;
  case 'v':
    num = '\v';
    count(1);
    break;
  case '\\':
    num = '\\';
    count(1);
    break;
  case '\'':
    num = '\'';
    count(1);
    break;
  case '\"':
    num = '\"';
    count(1);
    break;
  case '\?':
    num = '\?';
    count(1);
    break;
  default:
    if (is_digit(*lex.fid->c)) {
      num = *lex.fid->c;
      count(1);
      for (int i = 0; i < 2; i++) {
        if (is_digit(*lex.fid->c))
          num = num * 10 + *lex.fid->c;
        else
          break;
        count(1);
      }
    } else {
      token_error("invalid escape sequence");
    }
  }
  
  return num;
}

int read_constant()
{
  if (*lex.fid->c == '\'') {
    count(1);
    
    if (*lex.fid->c == '\'')
      token_error("empty character constant");
    
    if (*lex.fid->c == '\\') {
      lex.token_num = read_escape_sequence();
    } else {
      lex.token_num = *lex.fid->c;
      count(1);
    }
    
    if (*lex.fid->c != '\'')
      token_error("multi-character chararacter constant");
    
    count(1);
    
    lex.token = TK_CONSTANT;
    
    return 1;
  } else if (is_digit(*lex.fid->c)) {
    lex.token_num = to_digit(read_char());
    
    while (is_digit(*lex.fid->c))
      lex.token_num = lex.token_num * 10 + to_digit(read_char());
    
    lex.token = TK_CONSTANT;
    
    return 1;
  }
  
  return 0;
}

int keyword_match(keyword_t *keyword, const char *word)
{
  for (int i = 0; i < strlen(keyword->key); i++) {
    if (word[i] != keyword->key[i])
      return 0;
  }
  
  return 1; 
}

int read_word()
{
  static char word[MAX_WORD];
  char *letter = &word[0];
  
  if (is_letter(*lex.fid->c) || *lex.fid->c == '_') {
    *letter++ = read_char();
    
    while (is_letter(*lex.fid->c)
    || is_digit(*lex.fid->c)
    || *lex.fid->c == '_') {
      if (letter + 1 >= &word[MAX_WORD])
        error("word exceeded length MAX_WORD: '%i'.", MAX_WORD);
      
      *letter++ = read_char();
    }
    
    *letter++ = '\0';
    
    hash_t word_hash = hash_value(word);
    
    for (int i = 0; i < keyword_dict_count; i++) {
      if (keyword_match(&keyword_dict[i], word)) {
        lex.token = keyword_dict[i].token;
        return 1;
      }
    }
    
    lex.token = TK_IDENTIFIER;
    lex.token_hash = word_hash;
    
    return 1;
  }
  
  return 0;
}

int op_match(op_t *op)
{
  for (int i = 0; i < strlen(op->key); i++) {
    if (lex.fid->c[i] != op->key[i])
      return 0;
  }
  
  return 1;
}

int read_op()
{
  for (int i = 0; i < op_dict_count; i++) {
    if (op_match(&op_dict[i])) {
      count(strlen(op_dict[i].key));
      
      lex.token = op_dict[i].token;
      return 1;
    }
  }
  
  return 0;
}

int read_string_literal()
{
  if (*lex.fid->c != '"')
    return 0;
  
  count(1);
  
  int str_size = 32;
  
  char *str_buf = malloc(str_size);
  int str_pos = 0;
  while (*lex.fid->c != '"') {
    if (str_pos + 2 >= str_size) {
      str_size += 32;
      str_buf = realloc(str_buf, str_size);
    }
    
    if (*lex.fid->c == '\\') {
      str_buf[str_pos++] = read_escape_sequence();
    } else {
      str_buf[str_pos++] = *lex.fid->c;
      count(1);
    }
  }
  
  str_buf[str_pos] = '\0';
  
  count(1);
  
  lex.token_hash = hash_value(str_buf);
  lex.token = TK_STRING_LITERAL;
  
  free(str_buf);
  
  return 1;
}

int read_comment()
{
  if (lex.fid->c[0] == '/' && lex.fid->c[1] == '/') {
    count(2);
    while (*lex.fid->c != '\n') {
      count(1);
    }
    count(1);
  } else if (lex.fid->c[0] == '/' && lex.fid->c[1] == '*') {
    count(2);
    while (lex.fid->c[0] != '*' && lex.fid->c[1] != '/')
      count(1);
    count(2);
  } else {
    return 0;
  }
  
  return 1;
}

void reset_token()
{
  lex.token = 0;
  lex.token_num = 0;
  lex.token_hash = 0;
}

int sub_str_match(char *target, char *match)
{
  int len = strlen(match);
  
  int i = 0;
  while (*match && i < len) {
    if (target[i] != match[i])
      return 0;
    i++;
  }
  
  return 1;
}

char *filename()
{
  if (*lex.fid->c != '"')
    return NULL;
  
  count(1);
  
  int fname_size = 32;
  char *buf = malloc(fname_size);
  int pos = 0;
  
  while (*lex.fid->c != '"') {
    buf[pos++] = *lex.fid->c;
    count(1);
    
    if (pos + 1 >= fname_size) {
      fname_size += 32;
      buf = realloc(buf, fname_size);
    }
  }
  
  buf[pos++] = '\0';
  
  count(1);
  
  char *slash = strrchr(lex.fid->fname, '/');
  
  if (slash) {
    char *dir = strndup(lex.fid->fname, slash - lex.fid->fname);
    int len = strlen(dir);
    
    dir = realloc(dir, strlen(dir) + pos + 1);
    dir[len] = '/';
    strcat(dir + 1, buf);
    
    free(buf);
    
    buf = dir;
  }
  
  return buf;
}

void preprocess()
{
  if (sub_str_match(lex.fid->c, "#include ")) {
    count(strlen("#include "));
    
    char *fname = filename();
    if (!fname)
      token_error("#include expects \"FILENAME\"");
    
    FILE *in = fopen(fname, "rb");
    if (!in)
      token_error("could not open '%s'", fname);
    
    lexify(in, fname);
  }
}

void unlexify()
{
  fclose(lex.fid->file);
  free(lex.fid->fname);
  --lex.fid;
  lex.token = 0;
  next();
}

void next()
{
  reset_token();
  
  int prev_line_no = lex.fid->line_no;
  
  while (1) {
    switch (*lex.fid->c) {
    case EOF:
      if (lex.fid > lex.fstack + 1)
        unlexify();
      else
        lex.token = EOF;
      return;
    case '\n':
    case ' ':
    case '\t':
      count(1);
      break;
    case '"':
      read_string_literal();
      return;
    case '#':
      if (lex.fid->line_no != prev_line_no || lex.token == 0) {
        preprocess();
        return;
      }
    default:
      if (!read_comment()) {
        if (read_constant()
        || read_word()
        || read_op())
          return;
        
        token_error("unknown character: '%c (%i)', ignoring.", *lex.fid->c, *lex.fid->c);
        read_char();
        
        return;
      }
      break;
    }
  }
}

void lex_init()
{
  lex.fid = lex.fstack;
}

void lexify(FILE *file, char *fname)
{
  ++lex.fid;
  lex.fid->file = file;
  lex.fid->fname = fname;
  lex.fid->line_no = 1;
  
  lex.token = 0;
  lex.fid->c = &lex.fid->tmp_buf[MAX_TMP - 1];
  
  count(1);
  next();
}
