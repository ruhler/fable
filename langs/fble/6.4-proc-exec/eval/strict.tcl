fble-test {
  @ Unit@ = *();
  @ Maybe@ = +(Unit@! just, Unit@ nothing);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  # Proc exec is non-strict: the bindings are not evaluated until we run the
  # process itself.
  Unit@! _mku = {
    Unit@ u := nothing.just;
    $(u);
  };
  Unit@();
}
