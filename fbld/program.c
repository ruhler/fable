// program.c --
//   This file implements routines for working with loaded fbld programs.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fblc.h"
#include "fbld.h"

// FbldPrintQRef -- see documentation in fbld.h
void FbldPrintQRef(FILE* stream, FbldQRef* qref)
{
  fprintf(stream, "%s", qref->name->name);
  if (qref->paramv->size > 0) {
    fprintf(stream, "<");
    for (size_t i = 0; i < qref->paramv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FbldPrintQRef(stream, qref->paramv->xs[i]);
    }
    fprintf(stream, ">");
  }
  if (qref->mref != NULL) {
    fprintf(stream, "@");
    FbldPrintQRef(stream, qref->mref);
  }
}
