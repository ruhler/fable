set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func f(Unit x ; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func f(Unit x ; Unit) {
        y;
      };

      func main( ; Unit) {
        # The variable y should not be visible from the body of function 'f'.
        Unit y = Unit();
        f(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:6:9
