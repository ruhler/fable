// symbols.c --
//   This file implements routines for manipulating fblc symbol information,
//   which maps source level names and locations to machine level program
//   constructs.

#include <assert.h>     // for assert

#include "fblcs.h"

// DeclName -- See documentation in fblcs.h
Name DeclName(SProgram* sprog, FblcDeclId decl_id)
{
  assert(decl_id < sprog->program->declc);
  SDecl* sdecl = sprog->symbols[decl_id];
  return sdecl->name.name;
}

// FieldName -- See documentation in fblcs.h
Name FieldName(SProgram* sprog, FblcDeclId decl_id, FblcFieldId field_id)
{
  assert(decl_id < sprog->program->declc);
  FblcDecl* decl = sprog->program->declv[decl_id];
  assert(decl->tag == FBLC_STRUCT_DECL || decl->tag == FBLC_UNION_DECL);
  STypeDecl* stype = (STypeDecl*)sprog->symbols[decl_id];
  return stype->fields[field_id].name.name;
}

// LookupDecl -- See documentation in fblcs.h
FblcDeclId LookupDecl(SProgram* sprog, Name name)
{
  for (size_t i = 0; i < sprog->program->declc; ++i) {
    if (NamesEqual(sprog->symbols[i]->name.name, name)) {
      return i;
    }
  }
  return NULL_ID;
}
