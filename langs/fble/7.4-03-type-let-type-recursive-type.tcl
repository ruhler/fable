fble-test {
  # It should be possible to define recursive types using type let types.
  @ Unit@ = *();
  @ Num@ = {
    @ T@ = +(Unit@ Z, T@ S);
    T@;
  };
  Num@ two = Num@(S: Num@(S: Num@(Z: Unit@())));
  two;
}
