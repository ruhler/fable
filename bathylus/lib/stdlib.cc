
#include "bathylus/lib/stdlib.h"

StdLib::StdLib()
  : unit_t(&unit_t_), unit_e(&unit_e_), unit_v(Value::Struct(unit_t, {})),
    bit_t(&bit_t_), b0_e(&b0_e_), b1_e(&b1_e_),
    b0_v(Value::Union(bit_t, 0, unit_v)), b1_v(Value::Union(bit_t, 1, unit_v)),
    unit_t_(kStruct, "unit_t", {}),
    unit_e_(unit_t, {}),
    bit_t_(kUnion, "bit_t", {{unit_t, "0"}, {unit_t, "1"}}),
    b0_e_(bit_t, 0, unit_e),
    b1_e_(bit_t, 1, unit_e)
{}
