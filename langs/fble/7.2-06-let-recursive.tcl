fble-test {
  @ T@ = *(T@ i);

  # Recursive let
  { T@ x = T@(y), T@ y = T@(z), T@ z = T@(x); x; }.i.i.i.i.i.i.i.i;
}
