set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      func f(Unit x ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();

      func f(Unit x ; Unit) {
        x;
      };

      func main( ; Unit) {
        # The argument has invalid syntax.
        f(???);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:11:12
