#include "map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 1021

typedef struct entry_s entry_t;

struct entry_s {
  void *value;
  map_t map;
  entry_t *next;
  hash_t key;
};

static int map_id = 0;

static entry_t *entry_dict[MAX_ENTRIES];

map_t make_map()
{
  return map_id++;
}

entry_t *new_entry(int map, hash_t key, void *value)
{
  entry_t *entry = malloc(sizeof(entry_t));
  entry->map = map;
  entry->key = key;
  entry->value = value;
  entry->next = NULL;
  return entry;
}

void map_flush(map_t map)
{
  entry_t *prev_entry = NULL;
  for (int i = 0; i < MAX_ENTRIES; i++) {
    entry_t *entry = entry_dict[i];
    
    if (entry) {
      while (entry) {
        if (entry->map == map) {
          if (prev_entry)
            prev_entry->next = entry->next;
          else
            entry_dict[i] = entry->next;
          
          free(entry);
        }
        
        prev_entry = entry;
        entry = entry->next;
      }
    }
  }
}

int map_put(map_t map, hash_t key, void *value)
{
  int id = key % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry->next) {
      if (entry->key == key && entry->map == map)
        return 0;
      entry = entry->next;
    }
    
    entry->next = new_entry(map, key, value);
  } else {
    entry_dict[id] = new_entry(map, key, value);
  }
  
  return 1;
}

void *map_get(map_t map, hash_t key)
{
  int id = key % MAX_ENTRIES;
  
  entry_t *entry = entry_dict[id];
  
  if (entry) {
    while (entry) {
      if (entry->key == key && entry->map == map)
        return entry->value;
      entry = entry->next;
    }
  }
  
  return NULL;
}
