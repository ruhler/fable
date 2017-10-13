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
        f();    # Too few args to function 'f'.
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:10:9
