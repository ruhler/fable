set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      union Bool(Unit True, Unit False);
      func foo(Bool x ; Bool);
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      union Bool(Unit True, Unit False);

      func foo(Bool x ; Bool) {
        y;  # The variable 'y' has not been declared.
      };

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:7:9
