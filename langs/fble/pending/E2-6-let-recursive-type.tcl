fble-test {
  # It should be possible to define recursive types using lets.
  @ Unit = *();
  @ Num = +(Unit Z, Num S);
  Num two = Num@(S: Num@(S: Num@(Z: Unit@())));
  two;
}
