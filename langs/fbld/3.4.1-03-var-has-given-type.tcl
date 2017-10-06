set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      union Bool(Unit True, Unit False);
      func foo(Bool x ; Unit);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      union Bool(Unit True, Unit False);

      func foo(Bool x ; Unit) {
        x;  # The variable 'x' has type Bool, not type Unit.
      };

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9
