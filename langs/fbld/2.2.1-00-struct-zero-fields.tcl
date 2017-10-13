# A struct can be declared that contains no fields.
set prg {
  MainI.fbld {
    interf MainI {
      struct NoFields();
      func main( ; NoFields);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
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
