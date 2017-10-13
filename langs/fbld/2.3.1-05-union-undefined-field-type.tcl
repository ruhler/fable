set prg {
  MainI.fbld {
    interf MainI {
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      union A(Unit x, Donut y);   # Type Donut is not defined.

      func main( ; A) {
        A:x(Unit());
      };
    };
  }
}

fbld-check-error $prg MainM MainM.fbld:4:23
