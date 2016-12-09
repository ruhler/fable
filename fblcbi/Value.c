

// NewStructValue --
//
//  Allocate a new struct value.
//
// Inputs:
//   fieldc - The number of fields of the struct.
//
// Results:
//   A newly allocated struct value. The fields of the struct are left
//   uninitialized.
//
// Side effects:
//   Allocates a new struct value. Use Copy to make a (shared) copy of the
//   struct value and Release to release the resources associated with
//   the value.

StructValue* NewStructValue(size_t fieldc)
{
  StructValue* value = MALLOC(sizeof(StructValue) + fieldc * sizeof(Value*));
  value->refcount = 1;
  value->kind = fieldc;
  return value;
}

// NewUnionValue --
//
//  Allocate a new union value.
//
// Inputs:
//   None.
//
// Results:
//   A newly allocated union value. The field and tag of the union are left
//   uninitialized.
//
// Side effects:
//   Allocates a new union value. Use Copy to make a (shared) copy of the
//   union value, and Release to release the resources associated with
//   the value.

UnionValue* NewUnionValue()
{
  UnionValue* value = MALLOC(sizeof(UnionValue));
  value->refcount = 1;
  value->kind = UNION_KIND;
  return value;
}

// Copy --
//
//   Make a (likely shared) copy of the given value.
//
// Inputs:
//   src - The value to make a copy of.
//
// Results:
//   A copy of the value, which may be the same as the original value.
//
// Side effects:
//   None.

Value* FblcCopy(Value* src)
{
  src->refcount++;
  return src;
}

// Release --
//
//   Free the resources associated with a value.
//
// Inputs:
//   value - The value to free the resources of.
//
// Results:
//   None.
//
// Side effect:
//   The resources for the value are freed. The value may be NULL, in which
//   case no action is taken.

void Release(Value* value)
{
  if (value != NULL) {
    value->refcount--;
    if (value->refcount == 0) {
      if (value->kind == UNION_KIND) {
        UnionValue* union_value = (UnionValue*)value;
        Release(union_value->field);
      } else {
        StructValue* struct_value = (StructValue*)value;
        for (size_t i = 0; i < value->kind; ++i) {
          Release(struct_value->fieldv[i]);
        }
      }
      FREE(value);
    }
  }
}

static size_t SizeOfTag(size_t count)
{
  size_t size = 0;
  while ((1 << size) < count) {
    ++size;
  }
  return size;
}

Value* DecodeValue(Allocator* alloc, BitStream* bits, Program* prg, Type type)
{
  Decl* decl = prg->decls[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      StructDecl* struct_decl = (StructDecl*)decl;
      size_t fieldc = struct_decl->types.typec;
      StructValue* value = Alloc(alloc, sizeof(StructValue) + fieldc * sizeof(Value*));
      value->refcount = 1;
      for (size_t i = 0; i < fieldc; ++i) {
        value->fields[i] = DecodeValue(alloc, bits, prg, struct_decl->types.types[i]);
      }
      return value;
    }

    case UNION_DECL: {
      UnionDecl* union_decl = (UnionDecl*)decl;
      UnionValue* value = Alloc(alloc, sizeof(UnionValue));
      value->tag = ReadBits(bits, SizeOfTag(union_decl->types.typec));
      value->field = DecodeValue(alloc, bits, prg, union_decl->types.types[value->tag]);
      return value;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      return NULL;
  }
}

void EncodeValue(BitStream* bits, Program* prg, Type type, Value* value)
{
  Decl* decl = prg->decls[type];
  switch (decl->tag) {
    case STRUCT_DECL: {
      StructDecl* struct_decl = (StructDecl*)decl;
      StructValue* struct_value = (StructValue*)value;
      size_t fieldc = struct_decl->types.typec;
      for (size_t i = 0; i < fieldc; ++i) {
        EncodeValue(bits, prg, struct_decl->types.types[i], struct_value->fields[i]);
      }
      break;
    }

    case UNION_DECL: {
      UnionDecl* union_decl = (UnionDecl*)decl;
      UnionValue* union_value = (UnionValue*)value;
      WriteBits(bits, SizeOfTag(union_decl->types.typec), union_value->tag);
      EncodeValue(bits, prg, union_decl->types.types[value->tag], union_value->field);
      break;
    }

    default:
      assert(false && "type id does not refer to a type declaration");
      break;
  }
}

