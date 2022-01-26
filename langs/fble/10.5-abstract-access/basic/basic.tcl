fble-test {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);

    # Basic abstract access.
    @ Tok@ = %(/A%);
    @ AbsBool@ = Tok@<Bool@>;
    AbsBool@ t = Tok@.<AbsBool@>(Bool@(true: Unit));
    t.%.true;
  }
}
