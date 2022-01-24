fble-test {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);

    # Basic abstract cast, but with the package type renamed.
    # This is a regression test for a bug we had in the past
    @ Tok@ = %(/A%);
    @ TT@ = Tok@;
    @ AbsBool@ = TT@<Bool@>;
    AbsBool@ t = TT@.<AbsBool@>(Bool@(true: Unit));
    TT@.<Bool@>(t).true;
  }
}
