
#ifndef ADDER_H_
#define ADDER_H_

#include <memory>

class Component;

// Create an n-bit adder circuit.
// The circuit will have 2*n+1 inputs:
//  * The first n inputs are the bits of argument A, from most significant to
//    least significant bit.
//  * The second n inputs are the bits of argument B, from most significant to
//    least significant bit.
//  * The final input is the carry in.
// The circuit will have n+1 outputs:
//  * The first n bits of output are for the result A+B, from most significant
//    to least significant bit.
//  * The final bit of output is the carry out bit.
//
// n must be greater than 0.
std::unique_ptr<Component> CreateAdder(int n);

#endif//ADDER_H_
