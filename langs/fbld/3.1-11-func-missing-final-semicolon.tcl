set prg {
  MainI.fbld {
    mtype MainI {
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      # Function declarations must have a final semicolon.
      struct Unit();

      func f(Unit x; Unit) {
        x;
      }

      func main( ; Unit) {
        f();
      };
    };
  }
}
fbld-check-error $prg MainM MainM.fbld:10:7
