set prg {
  struct Unit();

  proc f(Unit+ myget ; ; Unit) {
    # myget port is a put port, not a get port.
    -myget();
  };
}

fblc-check-error $prg 6:6
