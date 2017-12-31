set prg {
  struct Unit();

  proc f(Unit- myput ; ; Unit) {
    # myput port has the wrong polarity.
    +myput(Unit());
  };
}

# TODO: Is this the best error location to use?
fblc-check-error $prg 6:6
