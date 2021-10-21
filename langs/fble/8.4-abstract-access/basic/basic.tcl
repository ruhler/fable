fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);

  # Basic abstract access.
  @? Tok@;
  @ AbsBool@ = Tok@<Bool@>;
  AbsBool@ t = Tok@(Bool@(true: Unit));
  t<Tok@>.true;
}
