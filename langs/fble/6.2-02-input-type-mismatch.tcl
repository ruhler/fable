fble-test-error 7:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ ~ get, put;
  -Bool@ myget = get;
  -Unit@ myget2 = myget;
  $(Unit@());
}
