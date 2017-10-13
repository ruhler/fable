set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      func main( ; Donut);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # Accessing the second component of a struct.
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; Donut) {
        A(Unit(), Donut()).y;
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Donut@MainM()
}
