#include "hash.h"

#include "map.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_STR 1024

static int str_size;
static char *str_buf;
static char *str_ptr;

static map_t str_map;

void hash_init()
{
  str_size = 1024;
  str_buf = malloc(str_size);
  str_ptr = str_buf;
  str_map = make_map();
}

void *str_alloc(char *value)
{
  int len = strlen(value);
  
  if (str_ptr + len >= &str_buf[str_size]) {
    printf("str_alloc(): ran out of memory\n");
    exit(-1);
  }
  
  char *ptr = str_ptr;
  memcpy(ptr, value, len + 1);
  str_ptr += len;
  
  *str_ptr++ = '\0';
  
  return ptr;
}

hash_t hash_value(char *value)
{  
  hash_t hash = 5381;
  
  char *c = value;
  while (*c)
    hash = ((hash << 5) + hash) + (unsigned char) *c++;
  
  if (!map_get(str_map, hash))
    map_put(str_map, hash, str_alloc(value));
  
  return hash;
}

char *hash_get(hash_t hash)
{
  return map_get(str_map, hash);
}
