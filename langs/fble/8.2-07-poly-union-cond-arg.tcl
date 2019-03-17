fble-test {
  # Regression test for passing an applied polymorphic type as an argument to
  # a condition.
  @ Unit@ = *();
  \<@; @> Maybe@ = \<@ T@> { +(T@ just, Unit@ nothing); };

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Maybe@<Unit@>; Bool@) isJust = \(Maybe@<Unit@> x) {
    # In a past bug, this failed with:
    #   expected value of union type, but found value of type Maybe<Unit>
    # Because we were not evaluating the type of x before checking it was a
    # union type.
    ?(x; just: true, nothing: false);
  };

  isJust(Maybe@<Unit@>(just: Unit@())).true;
}
