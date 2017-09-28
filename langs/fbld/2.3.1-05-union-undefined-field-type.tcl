set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      union A(Unit x, Donut y);   # Type Donut is not defined.

      func main( ; A) {
        A:x(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:4:23
