#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "common/error.h"
#include "cc/lex.h"
#include "cc/gen.h"
#include "cc/parse.h"
#include "vm/vm.h"

int main(int argc, char **argv)
{
  extern char *optarg;
  extern int optind;
  
  int c, err = 0;
  int flag_dump = 0;
  
  static char usage[] = "usage: %s [-dD] file\n";
  
  while ((c = getopt(argc, argv, "dD")) != -1) {
    switch (c) {
    case 'D':
      flag_dump = 1;
      break;
    case '?':
      err = 1;
      break;
    }
  }
  
  if ((optind+1) > argc) {
    fprintf(stderr, "%s: missing input file\n", argv[0]);
    fprintf(stderr, usage, argv[0]);
    exit(1);
  } else if (err) {
    fprintf(stderr, usage, argv[0]);
    exit(1);
  }
  
  char *fname = argv[optind];
  
  FILE *in = fopen(fname, "rb");
  if (!in) {
    fprintf(stderr, "%s: could not open %s\n", argv[0], fname);
    exit(1);
  }
  
  lex_init();
  hash_init();
  parse_init();
  
  lexify(in, fname);
  
  unit_t *unit = translation_unit();
  
  bin_t *bin = gen(unit);
  
  if (flag_dump)
    bin_dump(bin);
  
  vm_t *vm = make_vm();
  vm_load(vm, bin);
  
  vm_exec(vm);
  
  fclose(in);
  
  return 0;
}
