
def f(indent, var, values, default):
    if len(values) == 0:
        print("%s%s;" % (indent, default))
        return

    if len(values) == 1:
        for x in values:
            print("%s%s;" % (indent, values[x]))
        return

    print("%s%s.?(" % (indent, var))
    print("  %s1: %s," % (indent, values.get(1, default)))

    p0s = {}
    p1s = {}
    for x in values:
        if x == 1:
            continue
        elif x % 2 == 0:
            p0s[int(x / 2)] = values[x]
        else:
            p1s[int(x / 2)] = values[x]

    print("  %s2p0: {" % indent)
    if len(p0s) > 1:
        print("    %sIntP@ %s0 = %s.2p0;" % (indent, var, var))
    f("    " + indent, var + "0", p0s, default)
    print("  %s}," % indent)

    print("  %s2p1: {" % indent)
    if len(p1s) > 1:
        print("    %sIntP@ %s1 = %s.2p1;" % (indent, var, var))
    f("    " + indent, var + "1", p1s, default)
    print("  %s});" % indent)

ascii = {
        9:  "Chars.tab",
        10: "Chars.nl",
        13: "Chars.cr",
        32: "Chars.' '",
        33: "Chars.'!'",
        34: "Chars.'\"'",
        35: "Chars.'#'",
        36: "Chars.'$'",
        37: "Chars.'%'",
        38: "Chars.'&'",
        39: "Chars.''''",
        40: "Chars.'('",
        41: "Chars.')'",
        42: "Chars.'*'",
        43: "Chars.'+'",
        44: "Chars.','",
        45: "Chars.'-'",
        46: "Chars.'.'",
        47: "Chars.'/'",
        48: "Chars.'0'",
        49: "Chars.'1'",
        50: "Chars.'2'",
        51: "Chars.'3'",
        52: "Chars.'4'",
        53: "Chars.'5'",
        54: "Chars.'6'",
        55: "Chars.'7'",
        56: "Chars.'8'",
        57: "Chars.'9'",
        58: "Chars.':'",
        59: "Chars.';'",
        60: "Chars.'<'",
        61: "Chars.'='",
        62: "Chars.'>'",
        63: "Chars.'?'",
        64: "Chars.'@'",
        65: "Chars.A",
        66: "Chars.B",
        67: "Chars.C",
        68: "Chars.D",
        69: "Chars.E",
        70: "Chars.F",
        71: "Chars.G",
        72: "Chars.H",
        73: "Chars.I",
        74: "Chars.J",
        75: "Chars.K",
        76: "Chars.L",
        77: "Chars.M",
        78: "Chars.N",
79: "Chars.O",
80: "Chars.P",
81: "Chars.Q",
82: "Chars.R",
83: "Chars.S",
84: "Chars.T",
85: "Chars.U",
86: "Chars.V",
87: "Chars.W",
88: "Chars.X",
89: "Chars.Y",
90: "Chars.Z",
91: "Chars.'['",
92: "Chars.'\\'",
93: "Chars.']'",
94: "Chars.'^'",
95: "Chars.'_'",
96: "Chars.'`'",
97: "Chars.a",
98: "Chars.b",
99: "Chars.c",
100: "Chars.d",
101: "Chars.e",
102: "Chars.f",
103: "Chars.g",
104: "Chars.h",
105: "Chars.i",
106: "Chars.j",
107: "Chars.k",
108: "Chars.l",
109: "Chars.m",
110: "Chars.n",
111: "Chars.o",
112: "Chars.p",
113: "Chars.q",
114: "Chars.r",
115: "Chars.s",
116: "Chars.t",
117: "Chars.u",
118: "Chars.v",
119: "Chars.w",
120: "Chars.x",
121: "Chars.y",
122: "Chars.z",
123: "Chars.'{'",
124: "Chars.'|'",
125: "Chars.'}'",
126: "Chars.'~'",
}

f("    ", "p", ascii, "Char@(unknown: i)")
