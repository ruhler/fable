
# A field can have any type that is defined somewhere in the program.
# Which means it cannot have a type that is not defined.
set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct A(Unit x, Donut y);    # Type Donut not defined.
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct A(Unit x, Donut y);

      func main( ; Unit) {
        Unit();
      };
    };
  }
}

fbld-check-error $prg MainM MainI.fbld:4:24

