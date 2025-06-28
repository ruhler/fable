/**
 * @file program.c
 *  Routines for dealing with programs.
 */

#include "program.h"

#include <fble/fble-program.h>

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-vector.h>

#include "code.h"
#include "expr.h"

/**
 * @struct[Entry] An FbleModuleMap entry.
 *  @field[FbleModule*][key] The key.
 *  @field[void*][value] the value.
 */
typedef struct {
  FbleModule* key;
  void* value;
} Entry;

/**
 * @struct[EntryV] Vector of Entry.
 *  @field[size_t][size] Number of elements.
 *  @field[Entry*][xs] Elements.
 */
typedef struct {
  size_t size;
  Entry* xs;
} EntryV;

/**
 * @struct[FbleModuleMap] 
 *  See documentation in program.h
 *
 *  @field[EntryV][entries] The entries of the map.
 */
struct FbleModuleMap {
  EntryV entries;
};

// See documentation in program.h
FbleModuleMap* FbleNewModuleMap()
{
  FbleModuleMap* map = FbleAlloc(FbleModuleMap);
  FbleInitVector(map->entries);
  return map;
}

// See documentation in program.h
void FbleFreeModuleMap(FbleModuleMap* map, FbleModuleMapFreeFunction free_value, void* userdata)
{
  if (free_value != NULL) {
    for (size_t i = 0; i < map->entries.size; ++i)  {
      free_value(userdata, map->entries.xs[i].value);
    }
  }
  FbleFreeVector(map->entries);
  FbleFree(map);
}

// See documentation in program.h
void FbleModuleMapInsert(FbleModuleMap* map, FbleModule* key, void* value)
{
  Entry entry = { .key = key, .value = value };
  FbleAppendToVector(map->entries, entry);
}

// See documentation in program.h
bool FbleModuleMapLookup(FbleModuleMap* map, FbleModule* key, void** value)
{
  for (size_t i = 0; i < map->entries.size; ++i) {
    if (map->entries.xs[i].key == key) {
      *value = map->entries.xs[i].value;
      return true;
    }
  }
  return false;
}

// See documentation in fble-program.h.
void FbleFreeModule(FbleModule* module)
{
  FbleFreeModulePath(module->path);
  FbleFreeVector(module->type_deps);
  FbleFreeVector(module->link_deps);
  FbleFreeExpr(module->type);
  FbleFreeExpr(module->value);
  FbleFreeCode(module->code);
  for (size_t i = 0; i < module->profile_blocks.size; ++i) {
    FbleFreeName(module->profile_blocks.xs[i]);
  }
  FbleFreeVector(module->profile_blocks);
  FbleFree(module);
}

// See documentation in fble-program.h.
void FbleFreeProgram(FbleProgram* program)
{
  if (program != NULL) {
    for (size_t i = 0; i < program->modules.size; ++i) {
      FbleFreeModule(program->modules.xs[i]);
    }
    FbleFreeVector(program->modules);
    FbleFree(program);
  }
}
