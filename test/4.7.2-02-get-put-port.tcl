set prg {
  struct Unit();

  proc f(Unit ~> myget ; ; Unit) {
    # myget port is a put port, not a get port.
    ~myget();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

# TODO: Is this the right location for the error message?
fblc-check-error $prg 6:5
