fble-test-error 6:15 {
  @ Unit@ = *();
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  # Kind mismatch: expected poly type, but got poly value.
  <@>@ Foo@ = <@ T@> { Maybe@<T@>(nothing: Unit@()); };
  Unit@();
}
