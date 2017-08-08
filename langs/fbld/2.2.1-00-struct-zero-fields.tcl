# A struct can be declared that contains no fields.
set prg {
  Main.mtype {
    mtype Main<> {
      struct NoFields();
      func main( ; NoFields);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct NoFields();

      func main( ; NoFields) {
        NoFields();
      };
    };
  }
}

skip fbld-test $prg "main@Main<;>" {} {
  return NoFields@Main<;>()
}
