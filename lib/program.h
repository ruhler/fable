/**
 * @file program.h
 *  Helper functions for working with programs.
 */

#ifndef FBLE_INTERNAL_PROGRAM_H_
#define FBLE_INTERNAL_PROGRAM_H_

#include <fble/fble-program.h>   // for FbleModule.

/**
 * @type[FbleModuleMap] Map from FbleModule* to void*.
 */
typedef struct FbleModuleMap FbleModuleMap;

/**
 * @func[FbleNewModuleMap] Creates an empty FbleModuleMap.
 *  @sideeffects
 *   The user should call FbleFreeModuleMap on the result when done.
 */
FbleModuleMap* FbleNewModuleMap();

/**
 * @func[FbleFreeModuleMap] Frees resources associated with an FbleModuleMap.
 *  @arg[FbleModuleMap*][map] The map to free.
 *  @arg[void (*)(void*)][free_value] Function to free values on the map.
 *  @sideeffects
 *   @i Calls free_value on each map value, if free_value is not NULL.
 *   @i Frees resources associated with the map.
 */
void FbleFreeModuleMap(FbleModuleMap* map, void (*free_value)(void*));

/**
 * @func[FbleModuleMapInsert] Adds an entry into the map.
 *  @arg[FbleModuleMap*][map] The map to add an entry to.
 *  @arg[FbleModule*][key] The key for the entry.
 *  @arg[void*][value] The value for the entry.
 *  @sideeffects
 *   @i Adds the entry to the map.
 *   @i Behavior is undefined if the key is already present in the map.
 */
void FbleModuleMapInsert(FbleModuleMap* map, FbleModule* key, void* value);

/**
 * @func[FbleModuleMapLookup] Looks up the value for a module in the map.
 *  @arg[FbleModuleMap*][map] The map to look in.
 *  @arg[FbleModule*][key] The module whose value to look up.
 *  @arg[void**][value] Out param set to the value found if any.
 *  @returns[bool] True if the key was found, false otherwise.
 *  @sideeffects
 *   Sets value to the value of the entry in the map if one was found.
 */
bool FbleModuleMapLookup(FbleModuleMap* map, FbleModule* key, void** value);

#endif // FBLE_INTERNAL_PROGRAM_H_
