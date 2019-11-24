fble-test-error 6:12 {
  @ Unit@ = *();
  <@>@ M@ = <@ T@> { +(T@ just, Unit@ nothing); };

  # Kind mismatch: expected kind @, but found kind <@>@.
  @ Foo@ = M@;

  Unit@();
}
