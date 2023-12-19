/**
 * @file usage.c
 *  Implementation of usage routines.
 */

#include <fble/fble-usage.h>

#include <stdio.h>    // for fopen
#include <string.h>   // for strcpy, strrchr

#include "config.h"   // for FBLE_CONFIG_DOCDIR

const char* FbleDocDir = FBLE_CONFIG_DOCDIR "/fble";

static FILE* LocalPath(const char* arg0, const char* name);
static FILE* DocPath(const char* name);

/**
 * Returns a file stream to the usage info in the same directory as the
 * executable.
 *
 * @param arg0  The arg0 for the program.
 * @param name  The name of the usage doc.
 *
 * @returns
 *  A FILE pointer to the usage doc if found, NULL if not found.
 *
 * @sideeffects
 *  Opens a file that should be closed when no longer needed.
 */
static FILE* LocalPath(const char* arg0, const char* name)
{
  char path[strlen(arg0) + strlen(name) + 1];
  strcpy(path, arg0);
  char* tail = strrchr(path, '/');
  if (tail == NULL) {
    tail = path;
  } else {
    tail++;
  }
  strcpy(tail, name);
  return fopen(path, "r");
}

/**
 * Returns a file stream to the usage info in the fble doc directory.
 *
 * @param name  The name of the usage doc.
 *
 * @returns
 *  A FILE pointer to the usage doc if found, NULL if not found.
 *
 * @sideeffects
 *  Opens a file that should be closed when no longer needed.
 */
static FILE* DocPath(const char* name)
{
  char path[strlen(FbleDocDir) + strlen(name) + 2];
  strcpy(path, FbleDocDir);
  strcat(path, "/");
  strcat(path, name);
  return fopen(path, "r");
}

// See documentation in fble-usage.h
void FblePrintUsageDoc(const char* arg0, const char* name)
{
  FILE* fin = LocalPath(arg0, name);
  if (fin == NULL) {
    fin = DocPath(name);
  }

  if (fin == NULL) {
    printf("(no help found)\n"); 
    return;
  }

  int c = fgetc(fin);
  while (c != EOF) {
    putchar(c);
    c = fgetc(fin);
  }
  return;
}
