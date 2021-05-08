fble-test-compile-error 5:12 {
  @ Unit@ = *();

  # You are not allowed to use a variable to refer to a type of a type.
  @ Foo@ = @<Unit@>;
  Foo@;
}
