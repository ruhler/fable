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
      union MultiField(Unit x, Donut y);   # Should be 'struct', not 'union'.

      func main( ; MultiField) {
        MultiField:y(Donut());
      };
    };
  }
}

fbld-check-error $prg Main Main.mdefn:5:13
