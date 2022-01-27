fble-test {
  /A%;
} {
  A {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @ Bool@ = +(Unit@ true, Unit@ false);

    # Basic abstract value.
    @ Pkg@ = %(/A%);
    @ AbsBool@ = Pkg@<Bool@>;
    AbsBool@ t = Pkg@(Bool@(true: Unit));
    t.%.true;
  }
}
