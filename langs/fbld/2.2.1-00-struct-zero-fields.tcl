# A struct can be declared that contains no fields.
set prg {
  MainI.fbld {
    mtype MainI {
      struct NoFields();
      func main( ; NoFields);
    };
  }

  MainM.fbld {
    mdefn MainM(MainI) {
      struct NoFields();

      func main( ; NoFields) {
        NoFields();
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return NoFields@MainM()
}
