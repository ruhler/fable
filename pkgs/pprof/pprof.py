
import http.server
import sys

# Removes cycles of length n from the given trace
def RemoveCycles(trace, n):
    i = 0
    while i < len(trace):
        if trace[i:i+n] == trace[i+n:i+2*n]:
            for k in range(0, n):
                trace.pop(i+n)
        else:
            i += 1

def Canonicalize(trace):
    # To canonocalize a stack trace, any time we see a repeated subsequence
    # abc,abc in the trace, drop the second occurence.
    for i in range(1, len(trace)):
        RemoveCycles(trace, i)

# Returns the set of all non-empty subsequences of a given list of elements.
# Subsequences are returned as strings, which are the elements joined by ';',
# because lists aren't hashable...
def SubseqsOf(sequence):
    x = set()
    for i in range(0, len(sequence) - 1):
        for j in range(i+1, len(sequence)):
            x.add(';'.join(sequence[i:j]))
    return x

total = 0       # Total number of samples.
subseqs = {}    # Count 'overall' sample appearence of each frame.
selfs = {}      # Count 'self' sample apparence of each frame.
seqs = {}       # Full sequence to count of occurences.

frames = []
for line in sys.stdin:
    if line.startswith('\t'):
        [addr, name, so] = line.split()
        entry = name.split('+')[0]
        frames.append(entry)
    elif len(line.strip()) == 0:
        frames.reverse()
        Canonicalize(frames)
        seq = ';'.join(frames)
        seqs[seq] = 1 + seqs.get(seq, 0)

        for seq in SubseqsOf(frames):
            subseqs[seq] = 1 + subseqs.get(seq, 0)
        last = frames[-1]
        selfs[last] = 1 + selfs.get(last, 0)
        frames = []
        total += 1

# Compute incoming / outgoing graph.
incoming = {}   # subseq -> frame -> count
outgoing = {}   # subseq -> frame -> count
for seq in subseqs:
    split = seq.split(';')
    if len(split) <= 1:
        continue

    head = ';'.join(split[:-2])
    if head not in outgoing:
        outgoing[head] = {}
    outgoing[head][split[-1]] = subseqs[seq]

    tail = ';'.join(split[1:])
    if tail not in incoming:
        incoming[tail] = {}
    incoming[tail][split[0]] = subseqs[seq]

class PprofRequestHandler(http.server.BaseHTTPRequestHandler):
    def write(self, text):
        self.wfile.write(text.encode())

    def Overview(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

        self.write("<h1>Overview</h1>\n")
        self.write("Samples: %d<br />\n" % total)
        self.write("Stacks: %d<br />\n" % len(seqs))
        self.write("Frames: %d<br />\n" % len(selfs))
        self.write("<br />\n")
        self.write("<a href=\"/overall\">Frames by overall time</a><br />\n")
        self.write("<a href=\"/self\">Frames by self time</a><br />\n")
        self.close_connection = True

    def Overall(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.write("<h1>Frames by overall time</h1>\n")

        self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")

        by_overall = sorted(subseqs.keys(), key=lambda x: -subseqs[x])
        for seq in by_overall:
            if seq.find(';') >= 0:
                continue
            count = subseqs[seq]
            percent = 100.0 * count / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
            self.write("<td><a href=\"/seqs/%s\">%s</a></td></tr>\n" % (seq, seq))
        self.write("</table>\n")
        self.close_connection = True

    def Self(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

        self.write("<h1>Frames by self time</h1>\n")
        self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")

        for frame in sorted(selfs.keys(), key=lambda x: -selfs[x]):
            count = selfs[frame]
            percent = 100.0 * count / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
            self.write("<td><a href=\"/seqs/%s\">%s</a></td></tr>\n" % (frame, frame))
        self.write("</table>\n")

        self.close_connection = True

    def do_GET(self):
        if self.path == "/":
            self.Overview()
            return

        if self.path == "/overall":
            self.Overall()
            return

        if self.path == "/self":
            self.Self()
            return

        self.send_error(404, "invalid request")

if len(sys.argv) > 1 and sys.argv[1] == "--http":
    addr = ('localhost', 8123)
    httpd = http.server.HTTPServer(addr, PprofRequestHandler)
    httpd.serve_forever()

print("Overall")
print("=======")
by_overall = sorted(subseqs.keys(), key=lambda x: -subseqs[x])
n = 0
for seq in by_overall:
    if seq.find(';') >= 0:
        continue

    n += 1
    count = subseqs[seq]
    percent = 100.0 * count / float(total)
    if n > 10 and percent < 1.0:
        print("   ...")
        break
    print("%8.2f%% % 8d %s" % (percent, count, seq))

print("")
print("Self")
print("====")
n = 0
for frame in sorted(selfs.keys(), key=lambda x: -selfs[x]):
    n += 1
    count = selfs[frame]
    percent = 100.0 * count / float(total)
    if n > 10 and percent < 1.0:
        print("   ...")
        break
    print("%8.2f%% % 8d %s" % (percent, count, frame))

print("")
print("Graph")
print("====")
n = 0
for seq in by_overall:
    seq_total = subseqs[seq]
    percent = 100.0 * seq_total / float(total)

    n += 1
    if n > 10 and percent < 1.0:
        print("   ...")
        break

    print("")
    seq_incoming = incoming.get(seq, {})
    by_in = sorted(seq_incoming.keys(), key=lambda x: seq_incoming[x])
    for frame in by_in:
        count = seq_incoming[frame]
        print("%8.2f%% % 8d %s" % (100.0 * count / float(seq_total), count, frame))

    print("----")
    print("%8.2f%% % 8d %s" % (percent, seq_total, seq))
    print("----")

    seq_outgoing = outgoing.get(seq, {})
    by_out = sorted(seq_outgoing.keys(), key=lambda x: -seq_outgoing[x])
    for frame in by_out:
        count = seq_outgoing[frame]
        print("%8.2f%% % 8d %s" % (100.0 * count / float(seq_total), count, frame))
