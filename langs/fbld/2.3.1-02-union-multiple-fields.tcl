set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();
      union MultiField(Unit x, Donut y);
      func main( ; MultiField) {
        MultiField:x(Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return MultiField@MainM:x(Unit@MainM())
}
