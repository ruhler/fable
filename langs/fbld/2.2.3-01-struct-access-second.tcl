set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct Donut();
      func main( ; Donut);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
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

fbld-test $prg "main@Main" {} {
  return Donut@Main()
}
