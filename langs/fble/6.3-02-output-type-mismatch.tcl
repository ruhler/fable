fble-test-error 7:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ ~ get, put;
  +Bool@ myput = put;
  +Unit@ myput2 = myput;
  $(Unit@());
}
