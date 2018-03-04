set prg {
  func main<type Unit>( ; Unit) {
    # The Unit type cannot be constructed because it is an abstract type.
    Unit();
  };
}

fbld-check-error $prg 4:5
