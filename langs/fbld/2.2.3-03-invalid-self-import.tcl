set prg {
  # Module A is not in scope, you can't import from it.
  import A { A; };
}

fbld-check-error $prg 3:14
