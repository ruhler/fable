fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ ~ get, put;
  -Bool@ myget = get;
  -Bool@ myget2 = myget;
  $(Unit@());
}
