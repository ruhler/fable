fble-test-compile-error 11:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);

  @? Tok@;
  @ AbsBool@ = Tok@<Bool@>;

  # It's not legal to cast a Unit to a Bool@.
  Tok@.<AbsBool@>(Unit);
}
