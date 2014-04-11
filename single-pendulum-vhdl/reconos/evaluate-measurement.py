#!/usr/bin/env python

import re, glob, math

def find_in_file(pattern, filename):
    for line in open(filename, 'r'):
        m = re.search(pattern, line)
        if m:
            yield m.group(1)

def get_command(result_filename):
    command_line = [x for x in find_in_file(r"^Command: (.*)$", result_filename)]
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
        values = [float(value) for value in values]

        avg = calc_avg(values)
        stdev = calc_stdev(values, avg)

        yield (filename, command, avg, stdev)

def tsv(*cells):
    return "\t".join(map(str, cells)) + "\n"

with open("results.perf.txt", "w") as f:
    f.write(tsv("filename", "command", "avg", "stdev"))
    for result in get_results():
        f.write(tsv(*result))
