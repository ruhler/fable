fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of a poly is the same as the poly of a type.
  <@>@ A@ = <@ T@> { @<Unit>; };
  <@>@ B@ = @< <@ T@> { Unit; } >;

  (A@ x) {
    B@ y = x;
    y;
  };
}
