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
  if (qref->targv->size > 0 || qref->margv->size > 0) {
    fprintf(stream, "<");
    for (size_t i = 0; i < qref->targv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      FbldPrintQRef(stream, qref->targv->xs[i]);
    }

    if (qref->margv->size > 0) {
      fprintf(stream, ";");

      for (size_t i = 0; i < qref->margv->size; ++i) {
        if (i > 0) {
          fprintf(stream, ",");
        }
        FbldPrintQRef(stream, qref->margv->xs[i]);
      }
    }
    fprintf(stream, ">");
  }
  if (qref->mref != NULL) {
    fprintf(stream, "@");
    FbldPrintQRef(stream, qref->mref);
  }
}
