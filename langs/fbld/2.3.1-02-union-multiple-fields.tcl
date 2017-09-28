set prg {
  Main.mtype {
    mtype Main<> {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField) {
        MultiField:x(Unit());
      };
    };
  }
}

fbld-test $prg "main@Main<;>" {} {
  return MultiField@Main<;>:x(Unit@Main<;>())
}
