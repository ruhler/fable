fble-test {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # Basic union access.
  @ Maybe = +(Bool just, Unit nothing);
  Maybe x = Maybe@(just: true);
  x.just;
}
