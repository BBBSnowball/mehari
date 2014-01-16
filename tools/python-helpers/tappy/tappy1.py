"""
TAP producer for Python unittest framework

---

Copyright (c) 2011 Chris Packham

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
"""

import unittest
import sys

class TapTestResult(unittest.TestResult):
    """A test result class that produces TAP compliant output."""

    def __init__(self, stream, descriptions, verbosity):
        super(TapTestResult, self).__init__()
        self.stream = stream

    def startTest(self, test):
        super(TapTestResult, self).startTest(test)

    def addSuccess(self, test):
        super(TapTestResult, self).addSuccess(test)
        self.stream.write("ok %d - %s\n" % (self.testsRun, str(test)))

    def addFailure(self, test):
        super(TapTestResult, self).addFailure(test)
        self.stream.write("not ok - %d %s\n" % (self.testsRun, str(test)))

    def addError(self, test):
        super(TapTestResult, self).addError(test)
        self.stream.write("not ok %d - %s # ERROR\n" % (self.testsRun, str(test)))

    def addExpectedFailure(self, test, err):
        super(TapTestResult, self).addExpectedFailure(test, err)
        self.stream.write("not ok %d - %s # TODO known breakage\n" % (self.testsRun, str(test)))

    def addUnexpectedSuccess(self, test):
        super(TapTestResult, self).addUnexpectedSuccess(test)
        self.stream.write("ok %d - %s # TODO known breakage\n" % (self.testsRun, str(test)))

    def addSkip(self, test, reason):
        super(TapTestResult, self).addSkip(test, reason)
        self.stream.write("ok %d - %s # skip %s\n" % (self.testsRun, str(test), reason))

class TapTestRunner(unittest.TextTestRunner):
    """A test runner class that produces TAP compliant output."""

    resultclass = TapTestResult

    def __init__(self, stream=sys.stderr, descriptions=True, verbosity=1):
        super(TapTestRunner, self).__init__(stream, descriptions, verbosity)

    def _makeResult(self):
        return self.resultclass(self.stream, self.descriptions, self.verbosity)

    def run(self, test):
        self.stream.write("TAP version 13\n")
        self.stream.write("1..%d\n" % test.countTestCases())
        return super(TapTestRunner, self).run(test)

