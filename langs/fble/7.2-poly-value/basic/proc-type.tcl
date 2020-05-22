fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Test a polymorphic process type.
  <@>@ Get@ = <@ T@>{ T@!; };

  Bool@ ~ get, put;
  Get@<Bool@> _g = get;
  !(Unit@());
}
