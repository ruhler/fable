set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      # Arg missing type name.
      func f(x, Unit y; Unit) {
        x;
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:6:15
