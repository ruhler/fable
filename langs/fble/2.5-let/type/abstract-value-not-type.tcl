fble-test-compile-error 6:14 {
  @ Unit@ = *();

  <@ T@> (T@ t) {
    # Kind mismatch: expected type, but got abstract value.
    @ Foo@ = t;
    Unit@();
  };
}
