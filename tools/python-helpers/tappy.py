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
import traceback

class TapTestResult(unittest.TestResult):
    """A test result class that produces TAP compliant output."""

    def __init__(self, stream, descriptions, verbosity):
        super(TapTestResult, self).__init__()
        self.stream = stream

    def __formatErr(self, err):
        exctype, value, tb = err
        return ''.join(traceback.format_exception(exctype, value, tb))

    def __addDiagnostics(self, s):
        for line in s.splitlines():
            self.stream.write('# %s\n' % line)

    def addSuccess(self, test):
        super(TapTestResult, self).addSuccess(test)
        self.stream.write("ok %d - %s\n" % (self.testsRun, str(test)))

    def addFailure(self, test, err):
        super(TapTestResult, self).addFailure(test, err)
        self.stream.write("not ok - %d %s\n" % (self.testsRun, str(test)))
        self.__addDiagnostics(self.__formatErr(err))

    def addError(self, test, err):
        super(TapTestResult, self).addError(test, err)
        self.stream.write("not ok %d - %s # ERROR\n" % (self.testsRun, str(test)))
        self.tapOutput.append("# ERROR:")
        self.__addDiagnostics(self.__formatErr(err))

    def addExpectedFailure(self, test, err):
        super(TapTestResult, self).addExpectedFailure(test, err)
        self.stream.write("not ok %d - %s # TODO known breakage\n" % (self.testsRun, str(test)))
        self.__addDiagnostics(str(err))

    def addUnexpectedSuccess(self, test):
        super(TapTestResult, self).addUnexpectedSuccess(test)
        self.stream.write("ok %d - %s # TODO known breakage\n" % (self.testsRun, str(test)))

    def addSkip(self, test, reason):
        super(TapTestResult, self).addSkip(test, reason)
        self.stream.write("ok %d - %s # skip %s\n" % (self.testsRun, str(test), reason))

class MultiTestResult(unittest.TestResult):
    """A test result class that delegates to more than one TestResult instance."""

    def __init__(self, *test_results):
        super(MultiTestResult, self).__init__()
        self.__test_results = test_results

    def startTest(self, *args):
        super(MultiTestResult, self).startTest(*args)
        for inner in self.__test_results:
            inner.startTest(*args)

    def addSuccess(self, *args):
        super(MultiTestResult, self).addSuccess(*args)
        for inner in self.__test_results:
            inner.addSuccess(*args)

    def addFailure(self, *args):
        super(MultiTestResult, self).addFailure(*args)
        for inner in self.__test_results:
            inner.addFailure(*args)

    def addError(self, *args):
        super(MultiTestResult, self).addError(*args)
        for inner in self.__test_results:
            inner.addError(*args)

    def addExpectedFailure(self, *args):
        super(MultiTestResult, self).addExpectedFailure(*args)
        for inner in self.__test_results:
            inner.addExpectedFailure(*args)

    def addUnexpectedSuccess(self, *args):
        super(MultiTestResult, self).addUnexpectedSuccess(*args)
        for inner in self.__test_results:
            inner.addUnexpectedSuccess(*args)

    def addSkip(self, *args):
        super(MultiTestResult, self).addSkip(*args)
        for inner in self.__test_results:
            inner.addSkip(*args)

    def startTestRun(self, *args):
        super(MultiTestResult, self).startTestRun(*args)
        for inner in self.__test_results:
            inner.startTestRun(*args)

    def stopTestRun(self, *args):
        super(MultiTestResult, self).stopTestRun(*args)
        for inner in self.__test_results:
            inner.stopTestRun(*args)

class TapTestRunner(unittest.TextTestRunner):
    """A test runner class that produces TAP compliant output."""

    resultclass = TapTestResult
    tapfilename = "test.tap"

    def __init__(self, stream=sys.stderr, descriptions=True, verbosity=1):
        super(TapTestRunner, self).__init__(stream, descriptions, verbosity)

    def _makeResult(self):
        tapresult = self.resultclass(self.tapstream, self.descriptions, self.verbosity)
        if self.stream != None:
            return MultiTestResult(tapresult, unittest.TextTestResult(self.stream, self.descriptions, self.verbosity))
        else:
            return tapresult

    def run(self, test):
        with open(self.tapfilename, "w") as self.tapstream:
            self.tapstream.write("TAP version 13\n")
            self.tapstream.write("1..%d\n" % test.countTestCases())
            return super(TapTestRunner, self).run(test)

def unittest_main(tapfile="test.tap", *args, **kwargs):
    TapTestRunner.tapfilename = tapfile
    return unittest.main(*args, testRunner=TapTestRunner, **kwargs)
