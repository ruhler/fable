fble-test-error 4:15 {
  # The fields of an inline union type must be inline types.
  @ Unit@ = *();
  @ Foo@ = +$(Unit@ foo);
  Foo@;
}
