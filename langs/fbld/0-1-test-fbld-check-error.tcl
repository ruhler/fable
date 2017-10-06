set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();

      X X X X X X

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:5:7
