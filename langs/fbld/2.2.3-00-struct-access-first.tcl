set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      func main( ; Unit);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      # Accessing the first component of a struct.
      struct Unit();
      struct Donut();
      struct A(Unit x, Donut y);

      func main( ; Unit) {
        A(Unit(), Donut()).x;
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return Unit@MainM()
}
