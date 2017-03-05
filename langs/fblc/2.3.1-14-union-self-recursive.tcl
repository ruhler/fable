set prg {
  # A union can have a field with its same type.
  struct Unit();
  union Recursive(Unit x, Recursive y);

  func main( ; Recursive) {
    Recursive:y(Recursive:x(Unit()));
  };
}

fblc-test $prg main {} "return Recursive:y(Recursive:x(Unit()))"
