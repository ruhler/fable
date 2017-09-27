set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      # Accessing an unknown field of a struct.
      struct Unit();
      struct A(Unit x, Unit y);

      func main( ; Unit) {
        A(Unit(), Unit()).z;
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:8:27
