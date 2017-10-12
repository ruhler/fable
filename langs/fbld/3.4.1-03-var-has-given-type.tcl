set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      union Bool(Unit True, Unit False);
      func foo(Bool x ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func foo(Bool x ; Unit) {
        x;  # The variable 'x' has type Bool, not type Unit.
      };

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:7:9
