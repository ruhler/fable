
size_t SizeOfTag(size_t count)
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

