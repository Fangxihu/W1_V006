############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
This module enables users to run ACAT interactively.
"""
from __future__ import print_function
try:
    # Python 3
    import builtins
except ImportError:
    # Python 2
    import __builtin__ as builtins
import code
from collections import OrderedDict
import os
import sys
# It looks like nothing uses this, but it needs to be imported before
# readline.parse_and_bind() is called, in order to make tab completion work.
try:
    import readline
except ImportError:
    print("Install readline package if you want commandline editing & history")
    readline = None

from ACAT.Interpreter.Interpreter import Interpreter
from ACAT.Core import CoreUtils as cu
from ACAT.Core import Arch
from ACAT.Core import MulticoreSupport

def str_compare(str_a, str_b):
    """
    @brief Compares two string.
    @param[in] str_a First string.
    @param[in] str_b Second string.
    @param[out] Returns: -1 if str_a should be before str_b in the ordered list.
                          1 if str_a should be after str_b in the ordered list.
                          0 the wo string should be in the same position.

    This function is a simple string compare with the exception that "_" is
    counted as the last element to put private functions and variables at the
    end of the list.
    Note: The function also assumes that if the two function contains "." the
    part before the last "." could be ignored.
    """
    # ignore everything before the last "."
    str_a = str_a.split(".")[-1]
    str_b = str_b.split(".")[-1]

    # If only one of hte string contains "_" put the one with "_" as the later
    # element.
    if str_a.startswith("_") and not str_b.startswith("_"):
        return 1
    if not str_a.startswith("_") and str_b.startswith("_"):
        return -1

    # if non or both of the string contains "_" just use a simple string.
    # compair.
    if str_a == str_b:
        return 0
    if str_a > str_b:
        return 1

    return -1


def get_potential_object_names(object_dict, object_start_name):
    """
    @brief Returns the available variables of given a path (see example below).
    @param[in] object_dict Dictionary containing all the available object in
        the environment.
    @param[in] object_start_name The name of the object in the enviroment.
                This name can be a relative name for example:
                vaible1.attribute1.sub_attribute1

    Lets assume we have an environment with two variables each with a few
    attributes.
    variable1,
    variable1.attribute1
    variable1.attribute1.sub_attribute1,
    variable1.attribute1.sub_attribute2,
    variable1.attribute2,
    variable2,
    variable2.attribute1
    As we can see each attribute counts as a separate variable.

    This function will return all the potential variables with the given start
    name. For example if the given name is "" the function will search in the
    environment and finds two potential matches:
    variable1
    variable2

    On the other hand if the object_start_name is "variable1." the function will
    return the full name of the variable1 attributes which are:
    variable1.attribute1
    variable1.attribute2
    Therefore "variable1." is like a scope in the environment.

    Note. "variable1.blah" will return:
    variable1.attribute1
    variable1.attribute2
    because "blah" is not a valid of attribute variable1.
    """
    # the prefix will hold all
    prefix = ""
    attribute_path = object_start_name.split(".")
    for attr in attribute_path:
        # print "attr = ", attr
        if attr in list(object_dict.keys()):
            # add the attribute name to the variable
            prefix += attr + "."

            # save the local_object
            local_object = object_dict[attr]
            # now create a new environment with the attributes of the object.
            object_dict = {}
            for temp_attr in dir(local_object):
                object_dict[temp_attr] = local_object.__getattribute__(temp_attr)
        else:
            # exit recursive search.
            break

    ret_var_list = []

    # make a list from the current available environment.
    for i in object_dict.keys():
        # get the object attribute full name
        object_attr_name = prefix + i
        # add a parenthesis to mark the attribute as a function
        if callable(object_dict[i]):
            object_attr_name += "("
        # Add the  object_attr_name to the returned list.
        ret_var_list.append(object_attr_name)

    ret_var_list.sort(cmp=str_compare)
    return ret_var_list


def string_copleter(object_dict):
    """
    @brief Creates a completer function for the readline module.
    @param[in] analyses_names List of the name of the analyses
    @param[in] analyses_dict Dictionary containing all the analyses maped to the
                name.
    """

    def completer_func(text, state):
        """
        Command completer function for the readline module.
        """
        what_to_look = get_potential_object_names(object_dict, text)

        # and now filter out all the irrelevant matches.
        options = [i for i in what_to_look if i.startswith(text)]
        if state < len(options):
            return options[state]
        return None

    return completer_func


class HistoryConsole(code.InteractiveConsole):
    """
    @brief Class used enter to a interactive shell.
    """

    def __init__(self, local_vars):
        """
        @brief Initialises the console object
        @param[in] self Pointer to the current object
        @param[in] locals Local variables.
        """
        # call the parent constructor. Cannot use super because the class is
        # an old style object
        code.InteractiveConsole.__init__(self, locals=local_vars)

        # Enable auto completion if readline is installed in python.
        self.histfile = os.path.expanduser("~/.ACAT.history")
        if readline:
            # enable tab completion
            readline.parse_and_bind("tab: complete")
            readline.set_completer(
                string_copleter(object_dict=local_vars)
            )
            try:
                readline.read_history_file(self.histfile)
            except IOError:
                # Permission problems
                pass
        self.traceback = None

    def push(self, line):
        """Push a line to the interpreter.
        see code.InteractiveConsole.push for more.
        """
        # Save the history every time because there seems to be no other way
        # to ensure that history is kept when the session is Ctrl+C'd.
        if readline:
            try:
                readline.write_history_file(self.histfile)
            except IOError:
                # Permission problems
                pass

        # Give the line to the Python interpreter
        return code.InteractiveConsole.push(self, line)


#########################################


class Interactive(Interpreter):
    """
    @brief Class which encapsulates an interactive session.
    Kick things off by calling run_all().
    """

    def __init__(self, **kwargs):
        """
        @brief Initialises the interpreter object
        @param[in] self Pointer to the current object
        @param[in] **kwargs
        """
        Interpreter.__init__(self, **kwargs)
        # Load the analyses
        self._load_analyses()
        # save Arch for matlab
        self.arch = Arch

    def mem_print(self, address, words, processor=0, words_per_line=8):
        """
        @brief Get a slice of DM, starting from address and prints out the
        result in a nicely formatted manner.
        @param[in] address Address to start from.
        @param[in] words Number of words to read from the memory.
        @param[in] processor Processor to use. Default is processor 0.
        @param[in] words_per_line Number of words to print out. Default value
                                 is 8 words.
        """
        if processor == 0:
            content = self.p0.chipdata.get_data(
                address, words * Arch.addr_per_word
            )
        else:
            content = self.p1.chipdata.get_data(
                address, words * Arch.addr_per_word
            )

        # convert the memory content to an ordered dictionary
        mem_content = OrderedDict()
        for offset, value in enumerate(content):
            current_address = address + offset * Arch.addr_per_word
            mem_content[current_address] = value

        self.formatter.output(
            cu.mem_dict_to_string(
                mem_content,
                words_per_line))

    def load_bundle(self, bundle_path):
        """
        @brief Loads a bundle (also known as KDCs) and sets it for
        both processors.
        @param[in] bundle_path Path to the bundle name.
        """
        bundle = MulticoreSupport.load_boundle(bundle_path)
        self.p0.debuginfo.update_bundles(bundle)
        if cu.global_options.is_dualcore:
            self.p1.debuginfo.update_bundles(bundle)

    def run(self):
        """
        @brief Reads and run istruction from the command line.
        @param[in] self Pointer to the current object
        """
        # add the created analyses to the local variables. Use the processor
        # wanted by the user as the default.
        proc_name = "p" + str(cu.global_options.processor)
        processor = self.__getattribute__(proc_name)

        # store the variables available to the user to a dictionary
        user_variables = {}
        # add the current processors analyses.
        user_variables.update(processor.available_analyses)

        if cu.global_options.is_dualcore:
            user_variables["p0"] = self.p0
            user_variables["p1"] = self.p1

        # add chipdata, debuginfo and formatter to the local variables
        user_variables["chipdata"] = processor.chipdata
        user_variables["debuginfo"] = processor.debuginfo

        # Built-in functions.
        # We used to define 'ihelp' to avoid masking Python's built-in help
        # function. We don't need it any more, but keep it for now to avoid
        # confusing users.
        user_variables["get"] = processor.get
        user_variables["get_var"] = processor.get_var
        user_variables["get_reg"] = processor.get_reg
        user_variables["load_bundle"] = self.load_bundle

        # Make the default kymera elf id easily accessible
        user_variables["kymera_debuginfo"] = processor.debuginfo.get_kymera_debuginfo()
        user_variables["kymera_elf_id"] = processor.debuginfo.get_kymera_debuginfo().elf_id

        user_variables["ihelp"] = self.help
        user_variables["help"] = self.help
        user_variables["mem_print"] = self.mem_print
        user_variables["to_file"] = self.to_file

        # reveal some useful conversion functions
        from ..Core.CoreUtils import u32_to_s32
        from ..Core.CoreUtils import s32_to_frac32
        from ..Core.CoreUtils import u32_to_frac32
        user_variables["u32_to_s32"] = u32_to_s32
        user_variables["s32_to_frac32"] = s32_to_frac32
        user_variables["u32_to_frac32"] = u32_to_frac32

        # CoreUtils have some useful functions which a user may want.
        user_variables["cu"] = cu

        # We need to perform the ID check before people start typing in commands
        # and getting confusing answers.
        processor.sanitycheck.analyse_firmware_id()

        if cu.global_options.use_ipython:
            try:
                import IPython
                IPython.start_ipython(
                    argv=[], user_ns=user_variables,
                    display_banner=False,
                    check_same_thread=False
                )
                # Disable output to hide IPython issue which triggers
                # Sqlite ProgrammingError exception when embedding shell
                # in thread.
                # There is no code executed after exiting from ipython console
                # so it is safe to do this.
                devnull = open(os.devnull, "w")
                sys.stderr = devnull
            except ImportError:
                print("IPython not installed. -I option ignored!")
        else:
            history_console = HistoryConsole(local_vars=user_variables)
            # enter to interactive mode
            history_console.interact("Interactive mode - type help() for help.")

    def help(self, arg=None):
        """
        @brief Overloads the built-in help function.
        help() prints the help text for Interactive mode;
        help(foo) calls the standard help routine for thing 'foo'.
        @param[in] self Pointer to the current object
        @param[in] arg = None
        """
        if arg is not None:
            return builtins.help(arg)

        print(self._get_help_string())

    def _get_help_string(self):
        ret_string = """
    Audio Coredump Analysis Tool - Interactive mode

    Built-in commands
        help()                  : Prints this help text
        get()                   : Get a variable, register or range of
                                  addresses. Tries to be smart.
        mem_print(address, words): Get a slice of DM, starting from address
                                  and prints out the result in a nicely
                                  formatted manner.

    Built-in analysis is available via:\n"""
        for analyses in self.analyses:
            ret_string += \
                " " * 8 + analyses.lower() + " " * (24 - len(analyses)) + \
                ": Type help(" + analyses.lower() + ") for more info.\n"

        ret_string += """
    Other methods are available via:
        chipdata                : Encapsulates data from the chip/coredump
                                  Type help(chipdata) for more info.
        debuginfo               : Provides debug information
                                  Type help(debuginfo) for more info.

    Examples
        get('audio_slt_table')  : Get the variable named 'audio_slt_table'
        get('mm_doloop_start')  : Get the register named 'MM_DOLOOP_START'
        chipdata.get_data(0xd00, 30): Get a slice of DM, starting from address
                                  0xd00 and 30 addressable units. Alternatively
                                  use mem_print.
        get(0x406c10)           : Get the code entry at address 0x406c10
        to_file('file_name')    : Sets the output to a file.
        """
        return ret_string

##################################################
