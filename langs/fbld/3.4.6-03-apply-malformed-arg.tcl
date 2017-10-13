set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func f(Unit x; Unit);
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();

      func f(Unit x; Unit) {
        x;
      };

      func main( ; Unit) {
        # The variable 'x' has not been declared.
        f(x);
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:11:11
