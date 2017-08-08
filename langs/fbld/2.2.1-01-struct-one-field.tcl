# A struct can be declared that contains one field.
set prg {
  Main.mdecl {
    mdecl Main<> {
      struct Unit();
      struct OneField(Unit x);
      func main( ; OneField);
    };
  }

  Main.mdefn {
    mdefn Main< ; ; Main<>> {
      struct Unit();
      struct OneField(Unit x);

      func main( ; OneField) {
        OneField(Unit());
      };
    };
  }
}

skip fbld-test $prg "main@Main<;>" {} "return OneField@Main<;>(Unit@Main<;>())"
