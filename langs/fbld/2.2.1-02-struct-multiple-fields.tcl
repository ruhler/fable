# A struct can be declared that contains multiple fields.
set prg {
  Main.mdecl {
    mdecl Main() {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);

      func main( ; MultiField) {
        MultiField(Unit(), Donut());
      };
    };
  }
}

fbld-test $prg main@Main {} "return MultiField@Main(Unit@Main(),Donut@Main())"
