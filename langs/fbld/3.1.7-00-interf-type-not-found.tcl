set prg {
  interf I {
    # Unit has not declared in the interface.
    func main( ; Unit);
  };
}

fbld-check-error $prg 4:18
