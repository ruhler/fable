fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Bool@; Bool@) Not = \(Bool@ x) {
    ?(x; true: false, false: true);
  };

  # Basic test of a function application.
  Not[false].true;
}
