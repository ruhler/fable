fble-test {
  # It should be possible to define and use recursive polymorphic types when
  # the recursion is all done within a poly.
  <@>@ Num@ = <@ T@> {
    @ N@ = +(T@ Z, N@ S);
    N@;
  };

  @ Unit@ = *();
  Num@<Unit@> two = Num@<Unit@>(S: Num@<Unit@>(S: Num@<Unit@>(Z: Unit@())));
  two;
}
