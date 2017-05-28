# A struct can be declared that contains no fields.
set prg {
  Main.mdecl {
    mdecl Main() {
      struct NoFields();
      func main( ; NoFields);
    };
  }

  Main.mdefn {
    mdefn Main() {
      struct NoFields();

      func main( ; NoFields) {
        NoFields();
      };
    };
  }
}

fbld-test $prg main@Main {} "return NoFields@Main()"
