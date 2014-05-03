#!/usr/bin/env python

import re, glob, math, os, shutil

def tools_file(filename):
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), filename)

def find_in_file(pattern, filename):
    for line in open(filename, 'r'):
        m = re.search(pattern, line)
        if m:
            yield m.group(1)

def get_command(result_filename):
    command_line = list(find_in_file(r"^Command: (.*)$", result_filename))
    if len(command_line) >= 1:
        return command_line[0]
    else:
        return "???"

def calc_avg(values):
    if len(values) > 0:
        return sum(values) / len(values)
    else:
        return 0

def calc_stdev(values, avg):
    if len(values) <= 1:
        return "???"

    variance = sum((x - avg)**2 for x in values)
    n = len(values)
    return math.sqrt(variance / (n-1))

def get_results():
    for filename in glob.glob("*.perf.txt"):
        if filename == "results.perf.txt":
            continue

        command = get_command(filename)

        values = find_in_file(r"^\s*Calculation\s*:\s*([0-9]+)\s*ms$", filename)
        values = map(float, values)

        avg = calc_avg(values)
        stdev = calc_stdev(values, avg)

        yield (filename, command, avg, stdev, min(values), max(values), values)

def tsv(*cells):
    return "\t".join(map(str, cells)) + "\n"

def write_results_file():
    with open("results.perf.txt", "w") as f:
        f.write(tsv("filename", "command", "avg", "stdev", "min", "max"))
        for result in get_results():
            f.write(tsv(*result[0:-1]))

def frange(start, end, count):
    if count == 0:
        return []
    elif count == 1:
        return [start]

    xs = [None] * count
    for i in range(count):
        xs[i] = start + (end-start)/(count-1)*i

    return xs

def gnuplot_results():
    xs = [-0.2]
    ys = [0]
    with open("avg.dat", "w") as f:
        i = 0
        for filename, command, avg, stdev, min, max, values in get_results():
            n = len(values)
            xs.extend(frange(i + 0.1, i + 0.5, n))
            ys.extend(values)

            f.write(tsv(i, avg, stdev, min, max, "\"" + command.replace(" ", "\\n") + "\""))

            i += 1

    with open("data.dat", "w") as f:
        for x, y in zip(xs, ys):
            f.write("%f\t%f\n" % (x, y))

    shutil.copyfile(tools_file("result.gplot"), "result.gplot")
    os.system("gnuplot-nox result.gplot")

write_results_file()
if os.system("which gnuplot-nox >/dev/null") == 0:
    gnuplot_results()
