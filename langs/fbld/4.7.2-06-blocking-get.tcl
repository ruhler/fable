set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main(Unit- g, Unit+ p; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      # Test that execution will block if the last thread is blocking on a get
      # rather than return without finishing.
      # This test is inherently timing dependant, and may not work depending
      # on how things are scheduled. The idea is to have two threads run in
      # parallel, one of which blocks on a get before the other completes. The
      # other thread completes by doing a put, at which point only the get
      # remains. The hope is that if the implementation fails to block on the
      # get, it will fail before the test driver has time to read the last
      # value put to it and put the value to get.
      proc main(Unit- g, Unit+ p; ; Unit) {
        Unit x = -g(),
        Unit y = { Unit a = +p(Unit()); Unit b = +p(Unit()); +p(Unit()); };
        $(x);
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  get p Unit@MainM()
  get p Unit@MainM()
  get p Unit@MainM()
  put g Unit@MainM()
  return Unit@MainM()
}
