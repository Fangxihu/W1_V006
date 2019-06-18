############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2018 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Analysis which provides an interface for the FATS test system.
"""
import re
import os
from time import strftime
import traceback

from ACAT.Core import CoreTypes as ct
from . import Analysis
from ..Display.InteractiveFormatter import InteractiveFormatter


class Instruction(object):
    """
    Class for keeping an ACAT instruction.
    """
    def __init__(self, instr, state):
        self.instr = instr
        self.state = state
        self.instr_to_run = None


RESOURCE_USAGE_ANALYSES = ['profiler', 'heapmem', 'poolinfo']
OTHER_FATS_ANALYSES = ['debuglog']

AVAILABLE_INSTRUCTIONS = {
    "file (.*)": [Instruction(r"self.log_to_file(\1)", 'config')],
    "dbl_lvl (.*)": [
        Instruction(r"debuglog.set_debug_log_level(\1)", 'config'),
        Instruction("debuglog.poll_debug_log(False)", 'start'),
        Instruction("debuglog.stop_polling()", 'reset')
    ],
    # (Pedantry: FATS expects us to call this "MIPS", although it's
    # really about MCPS = millions of cycles per second.)
    "mips_clks (.*)": [
        Instruction(
            r"profiler.formatter.output('\\nSTART mips_clks');"
            r"profiler.run_clks(\1);"
            r"profiler.formatter.output('END mips_clks')",
            'read_resource_usage'
        )
    ],
    "mips_kymera_builtin": [
        Instruction(
            r"profiler.formatter.output('\\nSTART mips_kymera_builtin');"
            r"profiler.run_kymera_builtin();"
            r"profiler.formatter.output('END mips_kymera_builtin')",
            'read_resource_usage'
        )
    ],
    "mips_pc (.*)": [
        Instruction(
            r"profiler.formatter.output('\\nSTART mips_pc');"
            r"profiler.run_pc(\1);"
            r"profiler.formatter.output('END mips_pc')",
            'read_resource_usage'
        )
    ],
    "heap": [
        Instruction(
            r"heapmem.formatter.output('\\nSTART heap');"
            r"heapmem.get_watermarks();"
            r"heapmem.formatter.output('END heap');",
            'read_resource_usage'
        ),
        Instruction('heapmem.clear_watermarks()', 'reset')
    ],
    "pool": [
        Instruction(
            r"poolinfo.formatter.output('\\nSTART pool');"
            r"poolinfo.get_watermarks();"
            r"poolinfo.formatter.output('END pool')",
            'read_resource_usage'
        ),
        Instruction('poolinfo.clear_watermarks()', 'reset')
    ],
}


class Fats(Analysis.Analysis):
    """
    @brief This class encapsulates an analysis for resource usage used by FATS
        test.
    """

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)

    def run_all(self):
        """
        @brief This analyses cannot run in automatic mode. It is design to be
        used by the FATS system.
        @param[in] self Pointer to the current object
        """
        self.formatter.output(
            "FATS analysis does not support run_all() command"
        )

    def config(self, requests):
        """
        @brief Configure the Fats analysis and executes the config state
        specific instructions.
        @param[in] self Pointer to the current object
        @param[in] requests
        """
        try:
            if not isinstance(requests, tuple) and \
                not isinstance(requests, list):
                requests = [req.strip() for req in requests.split(";")]
        except BaseException:
            raise ct.AnalysisError("Configuration not recognised.")

        # go trough all request
        for request in requests:
            # translate the requests
            for config in AVAILABLE_INSTRUCTIONS:
                # print "-",config
                match = re.match(config, request)
                if match:
                    print(match.group())
                    try:
                        for instruction in AVAILABLE_INSTRUCTIONS[config]:
                            instruction.instr_to_run = re.sub(
                                config, instruction.instr, request
                            )
                    except BaseException:
                        for instruction in AVAILABLE_INSTRUCTIONS[config]:
                            instruction.instr_to_run = instruction.instr

        self.execute_state_commands("config")

    def start(self):
        """
        @brief Executes the start state specific instructions.
        @param[in] self Pointer to the current object
        """
        self.execute_state_commands("start")

    def read_resource_usage(self):
        """
        @brief Executes the read_resource_usage state specific instructions.
        @param[in] self Pointer to the current object
        """
        self.execute_state_commands("read_resource_usage")

    def reset(self):
        """
        @brief Test finished reset everything.
        @param[in] self Pointer to the current object
        """
        self.execute_state_commands("reset")

        # disable all instructions
        for config in AVAILABLE_INSTRUCTIONS:
            for instruction in AVAILABLE_INSTRUCTIONS[config]:
                instruction.instr_to_run = None

        # disable logging to file
        for analysis in OTHER_FATS_ANALYSES:
            self.interpreter.get_analysis(
                analysis, self.chipdata.processor
            ).reset_formatter()
        for analysis in RESOURCE_USAGE_ANALYSES:
            self.interpreter.get_analysis(
                analysis, self.chipdata.processor
            ).reset_formatter()

    def log_to_file(self, test_name=None):
        """
        @brief Change the output of all the FATS test to the file
        @param[in] self Pointer to the current object
        @param[in] test_name= None
        """
        if test_name is not None:
            logfile_name = os.path.splitext(test_name)[0]
        else:
            logfile_name = self.__class__.__name__

        new_logfile_name = logfile_name + strftime("_%Y_%m_%d__%H_%M_%S.log")

        formatter = InteractiveFormatter()
        formatter.change_log_file(new_logfile_name, True)

        for analysis in OTHER_FATS_ANALYSES:
            self.interpreter.get_analysis(
                analysis, self.chipdata.processor
            ).set_formatter(formatter)

        new_logfile_name = "%s_resource_usage.log" % (
            os.path.splitext(new_logfile_name)[0]
        )

        formatter = InteractiveFormatter()
        formatter.change_log_file(new_logfile_name, True)

        for analysis in RESOURCE_USAGE_ANALYSES:
            self.interpreter.get_analysis(
                analysis, self.chipdata.processor
            ).set_formatter(formatter)

    def execute_state_commands(self, state):
        """
        @brief Executes the state specific commands.
        @param[in] self Pointer to the current object
        @param[in] state
        """
        locals().update(self.interpreter.p0.available_analyses)
        for config in AVAILABLE_INSTRUCTIONS:
            for instruction in AVAILABLE_INSTRUCTIONS[config]:
                if instruction.instr_to_run is not None and instruction.state == state:
                    try:
                        exec (instruction.instr_to_run)
                    except BaseException:
                        self.formatter.output(traceback.format_exc() + '\n')

        self.formatter.output("fats.%s finished!" % state)
