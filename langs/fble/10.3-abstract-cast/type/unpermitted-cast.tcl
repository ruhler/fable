fble-test-compile-error 12:5 {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);

    @ Tok@ = %(/B%);
    @ AbsBool@ = Tok@<Bool@>;

    # Module /A% does not have permission to cast using package type /B%.
    Tok@.<AbsBool@>(True);
  }
}
