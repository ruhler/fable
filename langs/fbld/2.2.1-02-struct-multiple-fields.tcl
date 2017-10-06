# A struct can be declared that contains multiple fields.
set prg {
  Main.mtype {
    mtype Main {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);
      func main( ; MultiField);
    };
  }

  Main.mdefn {
    mdefn Main(Main) {
      struct Unit();
      struct Donut();
      struct MultiField(Unit x, Donut y);

      func main( ; MultiField) {
        MultiField(Unit(), Donut());
      };
    };
  }
}

fbld-test $prg "main@Main" {} {
  return MultiField@Main(Unit@Main(),Donut@Main())
}
