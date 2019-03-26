fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Test use of a polymorphic function.
  <@ T@>[T@]{T@;} Id = <@ T@>[T@ a] {
    a;
  };

  Id<Bool@>[true].true;
}
