fble-test-compile-error 8:8 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Tok@ = %(/Foo/Bar%);

  # Abstract type constructor should be passed a type, but Unit is not a type.
  Tok@<Unit>;
}
