fble-test {
  @ Unit@ = *();

  # A multi-arg poly is equivalent to its corresponding single arg
  # representation.
  <@, @>@ Multi@ = <@ A@, @ B@> { +(A@ a, B@ b, Unit@ c); };
  <@><@>@ Single@ = <@ A@><@ B@> { +(A@ a, B@ b, Unit@ c); };

  Multi@ multi = <@ A@, @ B@> { Multi@<A@, B@>(c: Unit@()); };
  Single@ single = multi;
  single;
}
