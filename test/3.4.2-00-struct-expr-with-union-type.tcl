
# A struct expression can't be used for a union type.

set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func main( ; Bool) {
    Bool(Unit(), Unit());
  };
}

fblc-check-error $prg

