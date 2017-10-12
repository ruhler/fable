set prg {
  MainI.fbld {
    mtype MainI {
      struct Unit();
      struct Donut();
      func main( ; Donut);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
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
