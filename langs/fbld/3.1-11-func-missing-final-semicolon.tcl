set prg {
  Main.mtype {
    mtype Main {
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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
fbld-check-error $prg Main Main.mdefn:10:7
