fble-test {
  # Test a function type polymorphic in its return type, for the purposes of
  # increasing code coverage.
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  <@>@ Maybe@ = <@ T@> { +(T@ just, Unit@ nothing); };

  @ Bool@ = +(Unit@ true, Unit@ false);

  <@>@ NothingFunc@ = <@ T@>(Unit@) { Maybe@<T@>; };
  @ BoolNothingFunc@ = NothingFunc@<Bool@>;

  BoolNothingFunc@ Nothing = (Unit@ u) {
    Maybe@<Bool@>(nothing: u);
  };

  Nothing(Unit).nothing;
}
