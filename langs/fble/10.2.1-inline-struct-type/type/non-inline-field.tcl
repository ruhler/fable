fble-test-error 5:16 {
  # The fields of an inline struct type must be inline types.
  @ Unit@ = *();
  @ Foo@ = *$(Unit@ foo);
  Foo@;
}
