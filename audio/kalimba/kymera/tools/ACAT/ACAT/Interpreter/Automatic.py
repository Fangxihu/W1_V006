############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
This module implements the automatic mode which runs all the analyses one after
the other.
"""
import traceback
import threading
import sys

from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from ACAT.Interpreter.Interpreter import Interpreter


class FunctionExecutionThread(threading.Thread):
    """
    @brief Class for running a function in a separate thread.
    """

    def __init__(self, function):
        threading.Thread.__init__(self)
        self.function = function
        self.exception = None

    def run(self):
        """
        @brief Starts the thread.
        @param[in] self Pointer to the current object
        """
        try:
            self.function()
        except BaseException:
            # Save the exception stack trace for the main thread.
            self.exception = ct.AnalysisError(traceback.format_exc())

    def stop(self):
        """
        @brief Stops the thread.
        @param[in] self Pointer to the current object
        """
        threading.Thread._Thread__stop(self)


#########################################
class Automatic(Interpreter):
    """
    @brief Class which encapsulates an automatic analysis session.
    """

    def __init__(self, **kwargs):
        """
        @brief Initialises the interpreter object
        @param[in] self Pointer to the current object
        @param[in] **kwargs
        """
        # Call the base Interpreter constructor.
        Interpreter.__init__(self, **kwargs)
        if 'analyses' in kwargs:
            self.analyses = kwargs.pop('analyses')
        # remove any unsupported analyses
        if "Fats" in self.analyses:
            self.analyses.remove("Fats")
        # Load the analyses
        self._load_analyses()

    def run(self):
        """
        @brief Runs all the available analyses for each processor.
        @param[in] self Pointer to the current object
        """
        # Sort the processor names
        processors = sorted(self.processors.keys())
        for proc in processors:
            self.formatter.set_proc(proc.upper())
            self.formatter.section_start(proc.upper())
            # check if the processor is booted
            if not self.processors[proc].is_booted():
                self.formatter.alert("Processor %s not booted!" % (str(proc)))
                self.formatter.section_end()
                continue
            for analysis in self.analyses:
                try:
                    # If the analysis initialised ok, try to run it.
                    analyses = self.processors[proc].available_analyses
                    analysis = analyses[analysis.lower()]
                    self._safe_run_analysis(analysis)
                except KeyError:
                    pass  # the analyses failed to initialise.
            self.formatter.section_end()

        # Once all analyses have run, flush the formatter so that we obtain
        # the juicy output.
        self.formatter.flush()

        # remove all references of the processors
        self.processors = []
        for proc in processors:
            setattr(self, proc, None)

    ##################################################
    # Private methods
    ##################################################
    def _safe_run_analysis(self, analysis):
        try:
            # timeout in seconds
            timeout = 120
            analysis_run_all_thread = FunctionExecutionThread(analysis.run_all)
            analysis_run_all_thread.start()
            analysis_run_all_thread.join(timeout)
            if analysis_run_all_thread.isAlive():
                analysis_run_all_thread.stop()
                stack_trace_str = analysis.__class__.__name__ + \
                    " got timeout after %d seconds" % timeout
                # Could not find a better way to access the stack of the
                # child thread so a protected method call to sys is
                # necessary
                # pylint: disable=protected-access
                stack = sys._current_frames()[analysis_run_all_thread.ident]
                # pylint: enable=protected-access
                for file_name, line_nomber, name, line in traceback.extract_stack(
                        stack):
                    stack_trace_str += '\nFile: "%s", line %d, in %s' % (
                        file_name, line_nomber, name
                    )
                    if line:
                        stack_trace_str += "\n  %s" % (line.strip())
                # print out the error and reset the sections
                self.formatter.error(stack_trace_str)
            elif analysis_run_all_thread.exception is not None:
                # Pylint error due to known pylint bug
                # https://www.logilab.org/ticket/3207
                # pylint: disable=raising-bad-type
                raise analysis_run_all_thread.exception
                # pylint: enable=raising-bad-type
        except ct.FatalAnalysisError as fae:
            # Fatal errors should exit immediately.
            self.formatter.alert(
                'Analysis ' + analysis.__class__.__name__ +
                ' failed to complete!' + '\n' + traceback.format_exc() + '\n'
            )
            self.formatter.flush()
            raise ct.FatalAnalysisError(str(fae))
        except Exception:  # pylint: disable=broad-except
            # Need to catch all error so I have to disable pylint's broad-except
            # error .
            # In automatic mode, we want to catch any other exceptions and try
            # to carry on, so that we can try all the other analyses.
            self.formatter.alert(
                'Analysis ' + analysis.__class__.__name__ +
                ' failed to complete!'
            )
            if not cu.global_options.under_test:
                self.formatter.output(traceback.format_exc() + '\n')
            self.formatter.section_reset()
