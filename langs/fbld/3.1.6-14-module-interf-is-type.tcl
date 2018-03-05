set prg {
  struct Unit();

  # Unit refers to a type. It cannot be used as the interface to a module.
  func main<module M(Unit)>( ; Unit) {
    Unit();
  };
}

fbld-check-error $prg 5:22
