set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # Missing entire argument from argument list.
      func f(Unit x, , Unit z; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:22
