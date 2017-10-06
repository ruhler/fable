set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      # Each field of a union type must have a different name.
      struct Unit();
      struct Donut();
      union BadStruct(Unit x, Donut x);

      func main( ; BadStruct) {
        BadStruct:x(Unit());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:6:37
