fble-test {
  # Test a basic use of polymorphic values.
  @ Unit@ = *();
  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };
  Maybe@ Nothing = <@ T@> { Maybe@<T@>(nothing: Unit@()); };

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Nothing<Bool@>.nothing;
}
