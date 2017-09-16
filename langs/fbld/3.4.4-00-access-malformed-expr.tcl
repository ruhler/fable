set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct A(Unit x, Unit y);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; Unit) {
        # The variable 'x' has not been declared.
        x.y;
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:8:9
