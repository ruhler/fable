// program.c --
//   This file implements routines for working with loaded fbld programs.

#include <assert.h>   // for assert
#include <stdlib.h>   // for NULL

#include "fblc.h"
#include "fbld.h"

static void PrintUserQRef(FILE* stream, FbldQRef* qref);
static void PrintInternalQRef(FILE* stream, FbldQRef* qref);


// PrintUserQRef
//   Print the user part of a resolved qref in human readable format to the
//   given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   qref - The qref to print.
//
// Results:
//   None.
//
// Side effects:
//   Prints the user part of the qref to the stream.
static void PrintUserQRef(FILE* stream, FbldQRef* qref)
{
  fprintf(stream, "%s", qref->name->name);
  if (qref->paramv->size > 0) {
    fprintf(stream, "<");
    for (size_t i = 0; i < qref->paramv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintUserQRef(stream, qref->paramv->xs[i]);
    }
    fprintf(stream, ">");
  }
  if (qref->mref != NULL) {
    fprintf(stream, "@");
    PrintUserQRef(stream, qref->mref);
  }
}

// PrintInternalQRef
//   Print the internal part of a resolved qref in human readable format to
//   the given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   qref - The qref to print.
//
// Results:
//   None.
//
// Side effects:
//   Prints the interna part of the qref to the stream.
static void PrintInternalQRef(FILE* stream, FbldQRef* qref)
{
  fprintf(stream, "%s", qref->name->name);
  if (qref->paramv->size > 0) {
    fprintf(stream, "<");
    for (size_t i = 0; i < qref->paramv->size; ++i) {
      if (i > 0) {
        fprintf(stream, ",");
      }
      PrintInternalQRef(stream, qref->paramv->xs[i]);
    }
    fprintf(stream, ">");
  }
  if (qref->r->mref != NULL) {
    fprintf(stream, "@");
    PrintInternalQRef(stream, qref->r->mref);
  }
}

// FbldPrintQRef -- see documentation in fbld.h
void FbldPrintQRef(FILE* stream, FbldQRef* qref)
{
  PrintUserQRef(stream, qref);
  fprintf(stream, " [");
  PrintInternalQRef(stream, qref);
  fprintf(stream, "]");
}
