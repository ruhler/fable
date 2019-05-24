fble-test-error 7:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  (Bool@+ output) {
    output();
  };
}


