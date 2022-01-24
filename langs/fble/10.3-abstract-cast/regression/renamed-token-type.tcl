fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);

  # Basic abstract cast, but with the token type renamed.
  # This is a regression test for a bug we had in the past
  @? Tok@;
  @ TT@ = Tok@;
  @ AbsBool@ = TT@<Bool@>;
  AbsBool@ t = TT@.<AbsBool@>(Bool@(true: Unit));
  TT@.<Bool@>(t).true;
}
