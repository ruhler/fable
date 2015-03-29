
#include "bathylus/lib/addition.h"

FullAdder::FullAdder(const StdLib& stdlib)
  : out_t(&out_t_), function(&function_),
    out_t_(kStruct, "FullAdderOut", {
        {stdlib.bit_t, "z"}, {stdlib.bit_t, "cout"}}),
    a_(stdlib.bit_t, "a"), b_(stdlib.bit_t, "b"), cin_(stdlib.bit_t, "cin"),
    z_(stdlib.bit_t, "z"), cout_(stdlib.bit_t, "cout"),
    not_cin_(&cin_, {stdlib.b1_e, stdlib.b0_e}),
    z_a0_b_(&b_, {&cin_, &not_cin_}),
    z_a1_b_(&b_, {&not_cin_, &cin_}),
    z_a_(&a_, {&z_a0_b_, &z_a1_b_}),
    cout_a0_b_(&b_, {stdlib.b0_e, &cin_}),
    cout_a1_b_(&b_, {&cin_, stdlib.b1_e}),
    cout_a_(&a_, {&cout_a0_b_, &cout_a1_b_}),
    result_(out_t, {&z_, &cout_}),
    let_({
        {stdlib.bit_t, "z", &z_a_},
        {stdlib.bit_t, "cout", &cout_a_}}, &result_),
    function_("FullAdder",
        {{stdlib.bit_t, "a"}, {stdlib.bit_t, "b"}, {stdlib.bit_t, "cin"}},
        out_t, &let_)
{}
