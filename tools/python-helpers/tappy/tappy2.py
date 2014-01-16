"""
TAP producer plugin for nosetests

Add this shell command to your builder ::

nosetests --with-tappy

By default, a file named nosetests.tap will be written to the
working directory.

This plugin is also available as a 'pure' unittest plugin at
github.com/cpackham/tappy.git

---

Copyright (c) 2011 Chris Packham and Tobi Wulff

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

import traceback
from nose.plugins.base import Plugin
from nose.exc import SkipTest

class Tappy(Plugin):
    """A test result class that produces TAP compliant output."""
    name = 'tappy'
    score = 2000
    encoding = 'UTF-8'

    def __init__(self):
        super(Tappy, self).__init__()

        self.testsRun = 0

        # Collect TAP output to be written to file later
        self.tapOutput = []
        self.tapOutput.append('TAP version 13')

        # N has to be replaced by the number of tests
        self.tapOutput.append('1..N')

    def options(self, parser, env):
        """Sets addition command line options"""
        super(Tappy, self).options(parser, env)
        parser.add_option(
            '--tap-file', action='store',
            dest='tap_file', metavar='FILE',
            default=env.get('NOSE_TAP_FILE', 'nosetests.tap'),
            help=('Path to TAP file to store test results.'
                  'Default is nosetests.tap in the working directory'
                  '[NOSE_TAP_FILE]'))

    def configure(self, options, config):
        """Configures the tappy plugin"""
        super(Tappy, self).configure(options, config)
        self.config = config
        self.tapFile = options.tap_file

    def report(self, stream):
        # Replace the placeholder "1..N" with actual number of tests
        self.tapOutput[1] = self.tapOutput[1].replace('1..N', '1..%d' % self.testsRun)

        # Write TAP output to file
        with open(self.tapFile, 'w') as file:
            file.write('\n'.join(self.tapOutput))
            file.write('\n')

        # Print some information about TAP output file
        if self.config.verbosity > 1:
            stream.writeln('-' * 70)
            stream.writeln('TAP output: %s' % self.tapFile)

    def startTest(self, test):
        self.testsRun += 1

    def addSuccess(self, test):
        self.tapOutput.append("ok %d - %s" % (self.testsRun, str(test)))

    def __formatErr(self, err):
        exctype, value, tb = err
        return ''.join(traceback.format_exception(exctype, value, tb))

    def __addDiagnostics(self, s):
        for line in s.splitlines():
            self.tapOutput.append('# %s' % line)

    def addFailure(self, test, err):
        self.tapOutput.append("not ok - %d %s" % (self.testsRun, str(test)))
        self.__addDiagnostics(self.__formatErr(err))

    def addError(self, test, err):
        self.tapOutput.append("not ok %d - %s" % (self.testsRun, str(test)))
        self.tapOutput.append("# ERROR:")
        self.__addDiagnostics(self.__formatErr(err))

    def addExpectedFailure(self, test, err):
        self.tapOutput.append("not ok %d - %s # TODO known breakage" % (self.testsRun, str(test)))
        self.__addDiagnostics(str(err))

    def addUnexpectedSuccess(self, test):
        self.tapOutput.append("ok %d - %s # TODO known breakage" % (self.testsRun, str(test)))

    def addSkip(self, test, reason):
        self.tapOutput.append("ok %d - %s # skip %s" % (self.testsRun, str(test), reason))