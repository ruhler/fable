fble-test {
  @ Unit@ = *();

  <@>@ P@ = <@ T@> { *(T@ head, S@<T@> tail); },
  <@>@ S@ = <@ T@> { +(P@<T@> cons, Unit@ nil); };

  # Test defining a polymorphic function that involves use of a recursive
  # polymorphic type. This is a regression test.
  <@ T@>(S@<T@>){T@;} _HeadS = <@ T@>(S@<T@> l) {
    l.cons.head;
  };

  Unit@();
}
