set prg {
  Main.mtype {
    mtype Main<> {
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # Two unions with different names are considered different types, even
      # if they have the same fields.
      struct Unit();

      union A(Unit x, Unit y);
      union B(Unit x, Unit y);

      func main( ; A) {
        B:x(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:11:9
