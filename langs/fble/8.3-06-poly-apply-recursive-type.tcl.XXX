fble-test {
  # It should be possible to define and use recursive polymorphic types.
  \<@;@> Num@ = \<@ T@> { +(T@ Z, Num@<T@> S); };
  @ Unit@ = *();
  Num@<Unit@> two = Num@<Unit@>(S: Num@<Unit@>(S: Num@<Unit@>(Z: Unit@())));
  two;
}
