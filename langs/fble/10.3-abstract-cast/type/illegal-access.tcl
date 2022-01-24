fble-test {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);

    # Basic abstract cast.
    @ Tok@ = %(/A%);
    @ AbsBool@ = Tok@<Bool@>;
    AbsBool@ t = Tok@.<AbsBool@>(Bool@(true: Unit));
    Tok@.<Bool@>(t).true;
  }
}
