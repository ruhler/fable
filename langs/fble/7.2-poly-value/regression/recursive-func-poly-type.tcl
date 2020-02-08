fble-test {
  # TODO: Uncomment the following to re-enable this test
  xxx

  @ Unit@ = *();

  <@>@ P@ = <@ T@> { *(T@ head, S@<T@> tail); },
  <@>@ S@ = <@ T@> { +(P@<T@> cons, Unit@ nil); };

  # Test defining a polymorphic function that involves use of a recursive
  # polymorphic type. This is a regression test.
  <@ T@>(S@<T@>){T@;} HeadS = <@ T@>(S@<T@> l) {
    l.cons.head;
  };

  Unit@();
}
