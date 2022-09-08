#include "bin.h"

#include <stdlib.h>

typedef enum tlump_e tlump_t;
typedef struct lump_s lump_t;
typedef struct header_s header_t;

enum tlump_e {
  LUMP_DATA,
  LUMP_INSTR,
  MAX_LUMP
};

struct lump_s {
  int fileofs;
  int filelen;
};

struct header_s {
  int bss_size;
  lump_t lumps[MAX_LUMP];
};

char *instr_tbl[] = {
  "push",
  "add",
  "sub",
  "mul",
  "div",
  "mod",
  "ldr",
  "ldr8",
  "str",
  "str8",
  "lbp",
  "enter",
  "leave",
  "call",
  "ret",
  "jmp",
  "cmp",
  "je",
  "jne",
  "jl",
  "jg",
  "jle",
  "jge",
  "sete",
  "setne",
  "setl",
  "setg",
  "setle",
  "setge",
  "sx8_32",
  "sx32_8",
  "int"
};

int num_instr_tbl = sizeof(instr_tbl) / sizeof(char *);

bin_t *make_bin(instr_t *instr, int num_instr, void *data, int data_size, int bss_size)
{
  bin_t *bin = malloc(sizeof(bin_t));
  bin->instr = instr;
  bin->num_instr = num_instr;
  bin->data = data;
  bin->data_size = data_size;
  bin->bss_size = bss_size;
  return bin;
}

void bin_dump(bin_t *bin)
{
  int i = 0;
  while (i < bin->num_instr) {
    switch (bin->instr[i]) {
    case PUSH:
    case ENTER:
    case CALL:
    case JMP:
    case JE:
    case JNE:
    case JL:
    case JG:
    case JLE:
    case JGE:
    case INT:
      printf("%03i %s %i\n", i, instr_tbl[bin->instr[i]], bin->instr[i + 1]);
      i += 2;
      break;
    default:
      printf("%03i %s\n", i, instr_tbl[bin->instr[i]]);
      i += 1;
      break;
    }
  }
}

void write_lump(FILE *out, header_t *header, void *src, int size, tlump_t tlump)
{
  header->lumps[tlump].fileofs = ftell(out);
  header->lumps[tlump].filelen = size;
  fwrite(src, 1, size, out);
}

void bin_write(bin_t *bin, FILE *out)
{
  fseek(out, sizeof(header_t), SEEK_SET);
  
  header_t header;
  header.bss_size = bin->bss_size;
  
  write_lump(out, &header, bin->data, bin->data_size, LUMP_DATA);
  write_lump(out, &header, bin->instr, bin->num_instr * sizeof(instr_t), LUMP_INSTR);
  
  fseek(out, 0, SEEK_SET);
  fwrite(&header, 1, sizeof(header_t), out);
}

void *copy_lump(FILE *in, header_t *header, int *size, tlump_t tlump)
{
  fseek(in, header->lumps[tlump].fileofs, SEEK_SET);
  
  *size = header->lumps[tlump].filelen;
  char *buffer = malloc(*size);
  fread(buffer, 1, *size, in);
  
  return buffer;
}

bin_t *bin_read(FILE *in)
{
  header_t header;
  fread(&header, 1, sizeof(header_t), in);
  
  int data_size;
  void *data = copy_lump(in, &header, &data_size, LUMP_DATA);
  
  int instr_size;
  instr_t *instr_buf  = copy_lump(in, &header, &instr_size, LUMP_INSTR);
  
  int num_instr = instr_size / sizeof(instr_t);
  
  return make_bin(instr_buf, num_instr, data, data_size, header.bss_size);
}
