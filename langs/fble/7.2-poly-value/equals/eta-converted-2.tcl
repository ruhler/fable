fble-test {
  <@>@ Id@ = <@ T@> { T@; };

  <<@>@ M@> (M@ m) {
    # M@ and N@ are the same type, even though one is a slightly complicated
    # eta-converted version of the other.
    <@>@ N@ = <@ A@> { Id@<M@<A@>>; };
    N@ n = m;
    n;
  };
}
