fble-test-compile-error 13:5 {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);

    @ Pkg@ = %(/B%);
    @ AbsBool@ = Pkg@<Bool@>;

    # Module /A% does not have permission to construct value using package
    # type /B%.
    Pkg@(Bool@(true: Unit));
  }
}
