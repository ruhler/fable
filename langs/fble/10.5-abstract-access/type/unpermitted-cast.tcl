fble-test-compile-error 6:5 {
  /B%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);

    @ Tok@ = %(/A%);
    @ AbsBool@ = Tok@<Bool@>;
    AbsBool@ AbsTrue = Tok@.<AbsBool@>(True);
    AbsTrue;
  }
} {
  B {
    % AbsTrue = /A%;

    # Module /B% does not have permission to access an abstract value
    # restricted with package type /A%.
    AbsTrue.%;
  }
}
