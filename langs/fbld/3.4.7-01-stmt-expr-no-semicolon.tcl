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

      func main( ; Unit) {
        # Missing a semicolon
        Unit()
      };
    };
  }
}
fbld-check-error $prg Main Main.mdefn:8:7
