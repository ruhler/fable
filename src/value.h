
#ifndef VALUE_H_
#define VALUE_H_

enum Bit { BIT_ZERO, BIT_ONE };

typedef Bit Value;

Bit unpack(const Value& v);
Value pack(const Bit& b);

#endif//VALUE_H_

