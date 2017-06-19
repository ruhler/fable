# A struct can be declared that contains no fields.
set prg {
  Main.mtype {
    mtype Main() {
      struct NoFields();
      func main( ; NoFields);
    };
  }

  Main.mdefn {
    mdefn Main(;;) {
      struct NoFields();

      func main( ; NoFields) {
        NoFields();
      };
    };
  }
}

fbld-test $prg Main(;) main {} {
  using Main(;) { NoFields; }
  return NoFields()
}
