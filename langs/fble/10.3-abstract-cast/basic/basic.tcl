fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);

  # Basic abstract cast.
  @? Tok@;
  @ AbsBool@ = Tok@<Bool@>;
  AbsBool@ t = Tok@.<AbsBool@>(Bool@(true: Unit));
  Tok@.<Bool@>(t).true;
}
