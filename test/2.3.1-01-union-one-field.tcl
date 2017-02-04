set prg {
  # A union can be declared that contains one field.
  struct Unit();
  union OneField(Unit x);

  func main( ; OneField) {
    OneField:x(Unit());
  };
}

fblc-test $prg main {} "return OneField:x(Unit())"
