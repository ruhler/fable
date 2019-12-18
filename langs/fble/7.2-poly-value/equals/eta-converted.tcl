fble-test {
  <<@>@ M@> (M@ m) {
    # M@ and N@ are the same type, even though one is an eta-converted
    # version of the other.
    <@>@ N@ = <@ A@> { M@<A@>; };
    N@ n = m;
    n;
  };
}
