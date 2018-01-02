set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      proc main( ; ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      proc put(Unit+ out; ; Unit) {
        +out(Unit());
      };

      proc main( ; ; Unit) {
        Unit +- p_put,  p_get;
        # p_get has the wrong polarity
        put(p_get; );
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:12:13
