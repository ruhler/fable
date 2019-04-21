fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ ~ get, put;
  Bool@+ myput = put;
  Bool@+ myput2 = myput;
  $(Unit@());
}
