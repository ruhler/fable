# A struct can be declared that contains multiple fields.
set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);

      func main( ; MultiField) {
        MultiField(Unit(), Donut());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return MultiField@MainM(Unit@MainM(),Donut@MainM())
}
