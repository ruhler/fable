# A struct can be declared that contains one field.
set prg {
  MainI.fbld {
    interf MainI {
      struct Unit();
      struct OneField(Unit x);
      func main( ; OneField);
    };
  }

  MainM.fbld {
    module MainM(MainI) {
      struct Unit();
      struct OneField(Unit x);

      func main( ; OneField) {
        OneField(Unit());
      };
    };
  }
}

fbld-test $prg "main@MainM" {} {
  return OneField@MainM(Unit@MainM())
}
