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
        # The variable name is missing.
        Unit = Unit();
        A(v, v);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:8:14
