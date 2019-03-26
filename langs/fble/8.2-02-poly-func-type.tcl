fble-test {
  # Test a polymorphic function type.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  <@>@ BinaryPredicate@ = <@ T@>[T@][T@]{Bool@;};
  @ BoolPredicate@ = BinaryPredicate@<Bool@>;

  BoolPredicate@ eqBool = [Bool@ a][Bool@ b] {
    ?(a; true: b,
         false: ?(b; true: false,
                     false: true));
  };

  eqBool[false][false].true;
}
