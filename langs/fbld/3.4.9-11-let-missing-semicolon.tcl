set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; A);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; A) {
        # The semicolon after the declaration is missing.
        Unit v = Unit()
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:9:9
