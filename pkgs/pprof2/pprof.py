
import http.server
import sys

# Returns the set of all non-empty subsequences of a given list of elements.
# Subsequences are returned as strings, which are the elements joined by ';',
# because lists aren't hashable...
def SubseqsOf(sequence):
    x = set()
    for i in range(0, len(sequence)):
        for j in range(i+1, len(sequence)+1):
            x.add(';'.join(sequence[i:j]))
    return x

total = 0       # Total number of samples.
subseqs = {}    # Count 'overall' sample appearence of each frame.
selfs = {}      # Count 'self' sample apparence of each frame.
seqs = {}       # Full sequence to count of occurences.
frames = set()  # Set of frames.
incoming = {}   # subseq -> frame -> count
outgoing = {}   # subseq -> frame -> count

class PprofRequestHandler(http.server.BaseHTTPRequestHandler):
    def write(self, text):
        self.wfile.write(text.encode())

    def log_message(self, format, *args):
        # don't log.
        None

    def Menu(self):
        self.write("<table><tr>")
        self.write("<td><a href=\"/\">Overview</a></td>")
        self.write("<td><a href=\"/overall\">Overall</a></td>")
        self.write("<td><a href=\"/self\">Self</a></td>")
        self.write("<td><a href=\"/seqs\">Sequences</a></td>")
        self.write("</tr></table><br />\n");

    def Overview(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        self.Menu()
        self.write("<h1>Overview</h1>\n")
        self.write("Samples: %d<br />\n" % total)
        self.write("Stacks: %d<br />\n" % len(seqs))
        self.write("Frames: %d<br />\n" % len(frames))
        self.close_connection = True

    def Overall(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        self.Menu()
        self.write("<h1>Frames by overall time</h1>\n")

        self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")

        by_overall = sorted(subseqs.keys(), key=lambda x: -subseqs[x])
        for seq in by_overall:
            if seq.find(';') >= 0:
                continue
            count = subseqs[seq]
            percent = 100.0 * count / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
            self.write("<td><a href=\"/seq/%s\">%s</a></td></tr>\n" % (seq, seq))
        self.write("</table>\n")
        self.close_connection = True

    def Self(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        self.Menu()
        self.write("<h1>Frames by self time</h1>\n")
        self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")

        for frame in sorted(selfs.keys(), key=lambda x: -selfs[x]):
            count = selfs[frame]
            percent = 100.0 * count / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
            self.write("<td><a href=\"/seq/%s\">%s</a></td></tr>\n" % (frame, frame))
        self.write("</table>\n")

        self.close_connection = True

    def Seqs(self):
        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        self.Menu()
        self.write("<h1>Full Sequences</h1>\n")
        self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")

        for seq in sorted(seqs.keys(), key=lambda x: -seqs[x]):
            count = seqs[seq]
            percent = 100.0 * count / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
            self.write("<td><a href=\"/seq/%s\">%s</a></td></tr>\n" % (seq, seq))
        self.write("</table>\n")

        self.close_connection = True

    def Seq(self, seq):
        if seq not in subseqs:
            self.send_error(404, "No such sequence.")
            return

        self.send_response(200, "OK")
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()


        self.Menu()
        self.write("<h1>Sequence</h1>\n")

        self.write("<table>\n")
        subseq = []
        for frame in seq.split(';'):
            subseq.append(frame)
            ss_total = subseqs[';'.join(subseq)]
            percent = 100.0 * ss_total / float(total)
            self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, ss_total))
            self.write("<td>%s</td></tr>\n" % frame)
        self.write("</table><br />\n")

        seq_total = subseqs[seq]

        if seq in incoming:
            self.write("<h1>Incoming</h1>\n")
            self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")
            seq_incoming = incoming[seq]
            by_in = sorted(seq_incoming.keys(), key=lambda x: -seq_incoming[x])
            for frame in by_in:
                count = seq_incoming[frame]
                percent = 100.0 * count / float(seq_total)
                self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
                self.write("<td><a href=\"/seq/%s;%s\">%s</a></td></tr>\n" % (frame, seq, frame))
            self.write("</table>\n")

        if seq in outgoing:
            self.write("<h1>Outgoing</h1>\n")
            self.write("<table>\n<tr><th>%</th><th>Count</th><th>Frame</th></tr>\n")
            seq_outgoing = outgoing[seq]
            by_in = sorted(seq_outgoing.keys(), key=lambda x: -seq_outgoing[x])
            for frame in by_in:
                count = seq_outgoing[frame]
                percent = 100.0 * count / float(seq_total)
                self.write("<tr><td>%.2f%%</td><td>%d</td>" % (percent, count))
                self.write("<td><a href=\"/seq/%s;%s\">%s</td></tr>\n" % (seq, frame, frame))
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

        if self.path == "/seqs":
            self.Seqs()
            return

        if self.path.startswith("/seq/"):
            self.Seq(self.path[5:])
            return

        self.send_error(404, "invalid request")

addr = ('localhost', 8123)
httpd = http.server.HTTPServer(addr, PprofRequestHandler)

for line in sys.stdin:
    [count, seq] = line.split()
    count = int(count)
    sample = seq.split(';')
    seqs[seq] = count + seqs.get(seq, 0)
    for seq in SubseqsOf(sample):
        subseqs[seq] = count + subseqs.get(seq, 0)
    last = sample[-1]
    selfs[last] = count + selfs.get(last, 0)
    sample = []
    total += count
    print("\rsamples: %d" % total, end='', flush=True)

print("")

for seq in subseqs:
    split = seq.split(';')
    if len(split) <= 1:
        continue

    head = ';'.join(split[:-1])
    if head not in outgoing:
        outgoing[head] = {}
    outgoing[head][split[-1]] = subseqs[seq]

    tail = ';'.join(split[1:])
    if tail not in incoming:
        incoming[tail] = {}
    incoming[tail][split[0]] = subseqs[seq]


print("Serving on http://localhost:8123")
httpd.serve_forever()
