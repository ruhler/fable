
# A field can have any type that is defined somewhere in the program.
# Which means it cannot have a type that is not defined.
set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct A(Unit x, Donut y);    # Type Donut not defined.
      func main( ; Unit);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct A(Unit x, Donut y);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg Main Main.mtype:4:24

