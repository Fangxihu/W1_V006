############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module holding all the helper functions and classes in ACAT.
"""
try:
    # Python 3
    import builtins
    from queue import Queue
except ImportError:
    # Python 2
    import __builtin__ as builtins
    from Queue import Queue
import imp
import sys
import os
import getopt
import threading
from collections import OrderedDict
import io as si


try:
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
except ImportError:
    # matplotlib not installed
    plt = None

from ACAT.Core import Arch
from .._version import __version__ as version
##########################################################################
#                               Globals
##########################################################################
# Is debug logging turned on?
DEBUG_LOG_ENABLED = True


def debug_log(log):
    """
    @brief print something out if debug is on
    @param[in] log
    """
    if DEBUG_LOG_ENABLED:
        print(log)

# Replace all instances of cu.INTEGERS when removing
# support for Python 2 from ACAT.
try:
    # Python 2
    INTEGERS = (int, long)
except NameError:
    # Python 3
    INTEGERS = (int)

# Command-line options are all stored globally, since some other modules
# will want to use them (and it's not good to pass them through long chains
# of constructors).
class GlobalOptions(object):
    """
    @brief A class to encapsulate all of the options supplied on the command-line
    (since Python doesn't really like globals).
    No members are defined here; they're part of the top-level script (e.g. ACAT.py)
    """
    def __init__(self):
        self.build_output_path_p0 = ""
        self.kymera_p0 = None
        self.build_output_path_p1 = ""
        self.kymera_p1 = None
        self.bundle_paths = None
        self.bundles = None
        self.patch = None
        self.wait_for_proc_to_start = False

        self.coredump_path = ""
        self.html_path = ""
        self.pythontools_path = ""  # path to the pythontools
        self.interactive = False
        self.use_ipython = False
        # We are analysing a live chip not a coredump (i.e. over SPI)
        self.live = False
        self.spi_trans = "lpt"
        self.processor = 0  # Default to p0
        self.build_mismatch_allowed = False
        self.is_dualcore = False
        self.kalcmd_object = None
        self.ker = None
        self.kmem = None  # kalmemaccessors
        self.kal = None
        self.kal2 = None
        self.dependency_check = False
        self.cache_validity_interval = 3.0
        # test related globals.
        self.under_test = False
        self.plotter_dict = None

# this is not a constant hence the lower case characters
global_options = GlobalOptions()  # pylint: disable=invalid-name

##########################################################################
#                      Internal Heper Funcnctions
##########################################################################

def create_counter(initial):
    """
    @brief increment the counter if it exists otherwise create a counter
    @param[in] initial value of the counter which has been hard coded to 0
    @param[out] return incremented output parameter
    """

    def counter():
        """
        Function which returns incremental numbers.
        """
        retval = counter.i
        counter.i += 1
        return retval

    counter.i = initial
    return counter


# initialise the create_counter function and use get_non_rep_nuber where required
# to get next index
get_non_rep_nuber = create_counter(initial=0)  # pylint: disable=invalid-name

def reset_package():
    """
    Resets every global ACAT variable.
    """
    global global_options, get_non_rep_nuber
    global_options = GlobalOptions()
    get_non_rep_nuber = create_counter(initial=0)
    Arch.chip_clear()



def run_plotter(pt_input):
    """
    Runs matplotlib plotter.
    """
    if plt is None:
        return "matplotlib.pyplot not available"
    title_string = builtins.str(pt_input["title_string"])
    conversion_function = pt_input["conversion_function"]
    buffer_dict = pt_input["buffer_dict"]
    output_format = pt_input["output_format"]
    # calling matplotlib... this can block if there is any threading issues.
    fig = plt.figure(figsize=(10, 5))
    subplot_axes = fig.add_subplot(111)
    # use tight_layout to save space, get rid of the white borders
    # plt.tight_layout()
    plt.grid(alpha=0.2)
    markers = []
    converted_values = OrderedDict()
    for key in buffer_dict:
        if isinstance(key, str):
            # for markers the value is the address
            marker_address = buffer_dict[key]
            marker_name = key
            markers.append([marker_name, marker_address])
        # make the conversion if needed
        if conversion_function:
            converted_values[key] = conversion_function(buffer_dict[key])

    # if no conversion was done the converted values will be the same as
    # the one returned form _get_content
    if not conversion_function:
        converted_values = buffer_dict
    # remove the markers to avoid failures when plotting the content
    for marker_name, marker_address in markers:
        converted_values.pop(marker_name)

    subplot_axes.plot(
        list(converted_values.keys()),
        list(converted_values.values()),
        label="waveform"
    )
    bottom, top = subplot_axes.get_ylim()
    for marker_name, marker_address in markers:
        # read and write pointer marker should be distinguished from other
        # markers
        if marker_name == "read pointer":
            # config for the vertical line
            line_address = marker_address + 1
            linestyle = 0, (1, 1)  # dotted 1-dot, 3-space
            # text config
            alignment = 'left'
            verticalalignment = 'top'
            color = 'green'
            y_location = top
        elif marker_name == "write pointer":  # config for the vertical line
            line_address = marker_address - 1
            linestyle = 0, (1, 2)  # dotted 1-dot, 3-space
            # text config
            alignment = 'right'
            color = 'red'
            verticalalignment = 'bottom'
            y_location = bottom
        else:
            # Default mark up configuration.
            # config for the vertical line
            line_address = marker_address
            linestyle = 0, (1, 3)  # dotted 1-dot , 3-space
            # text config
            alignment = 'right'
            color = 'orange'
            verticalalignment = 'top'
            y_location = top
        # add some space at both two sides.
        marker_name = "  " + marker_name + "  "
        subplot_axes.axvline(
            x=line_address,
            color=color,
            linestyle=linestyle
        )
        # display the marker
        subplot_axes.text(
            marker_address, y_location, marker_name,
            style='normal',  # style must be normal, italic or oblique
            ha=alignment,
            va=verticalalignment,
            bbox={'facecolor':color, 'alpha':0.2, 'pad':0}
        )

    axes = plt.gca()
    # provide a function which displays the addresses
    axes.get_xaxis().set_major_formatter(
        ticker.FuncFormatter(
            lambda x, _:" 0x%08x " % int(x)
        )
    )
    plt.legend(loc='lower right')
    plt.title("Buffer Content in " + title_string)
    plt.xlabel("Addresses")
    plt.ylabel("Values in " + conversion_function.__name__)
    if output_format == "svg":
        imgdata = si.StringIO()
        plt.savefig(imgdata, format='svg')
        imgdata.seek(0)  # rewind the data
        svg_dta = imgdata.getvalue().encode(errors="ignore")
        # closet the figure
        plt.close(fig)
        return svg_dta
    elif output_format == "window":
        plt.show()
        # closet the figure
        plt.close(fig)
        return None

class MatplotlibPlotter(threading.Thread):
    """
    Thread which runs run_plotter
    """
    def __init__(self, input_q, output_q):
        threading.Thread.__init__(self)
        self.input_q = input_q
        self.output_q = output_q

    def run(self):
        """
        Reads the input queue and put the result to the output queue
        """
        while True:
            pt_input = self.input_q.get()
            if pt_input is None:
                # And now exit from execution.
                return
            # call the runner function
            pt_output = run_plotter(pt_input)
            self.output_q.put(pt_output)



def hex(value):
    """
    @brief Convert an integer or list of integers to a hex string.
    This overrides the built-in function which adds a trailing
    'L' to large numbers. This version also recurses down an array.
    If passed anything that's not an int or array, we simply return its string
    representation.
    """
    if isinstance(value, (list, tuple)):
        return_val = builtins.str([hex(intgr) for intgr in value])
    elif isinstance(value, INTEGERS):
        return_val = builtins.str(builtins.hex(value).rstrip('L'))
    else:
        return_val = builtins.str(value)

    return return_val


def strip_elf_addr(addr, kal_arch):
    """
    @brief Addresses stored in the elf file have 32 bits, while the processor sees
    24 bits. Often, the 'invisible' top 8 bits are used by the toolchain
    to store some flags. For example, code address 0x400000 is stored in
    the elf as 0x81400000, and const address 0xa26000 is stored as 0x01a26000.
    This function strips off the 'invisible' bits of an elf-derived address.

    Kalarch4: for PM addresses the 31 bit is set (and the 30th bit is always unset)
    in the elf file, therefore the 31 bit is stripped.
    @param[in] addr
    @param[in] kal_arch
    """
    if kal_arch == 4 and (addr >> 31) & 0x1 and not (addr >> 30) & 0x1:
        return addr & 0x7FFFFFFF

    return addr & 0x00FFFFFF


def swap_endianness(num, number_width):
    """
    @brief Takes an int/long of arbitrary length, and returns it with bytes
        reversed. In other words it does a endianness swap.
    Examples:
        swap_endianness(0x1234, 2) -> 0x3412
        swap_endianness(0x1234, 4) -> 0x34120000
        swap_endianness(0xaaa, 2) -> 0xaa0a
    @param[in] num The number
    @param[in] number_width The width of the number in bytes.
    @param[out] The endiannes swapped number.
    """
    if number_width == 1:
        return num & 0x000000FF
    elif number_width == 2:
        return (
            ((num << 8) & 0xFF00) |
            ((num >> 8) & 0x00FF)
        )
    elif number_width == 4:
        return (
            ((num << 24) & 0xFF000000) |
            ((num << 8) & 0x00FF0000) |
            ((num >> 8) & 0x0000FF00) |
            ((num >> 24) & 0x000000FF)
        )
    else:
        raise TypeError(
            "Cannot change endianness for a variable "
            "with length %d bytes." % (number_width)
        )


def inspect_bitfield(value, parity, size_bits=24):
    """
    @brief Takes an int representing a bitfield, and returns a tuple containing:
    * parity = True: a list of which bits are set
    * parity = False: a list of which bits are not set (requires 'size_bits'
      to be set)
    In both cases, an empty list can be returned.
    Examples:
      inspect_bitfield(0xf01, True) -> (0, 8, 9, 10, 11)
      inspect_bitfield(0xf01, False, 12) -> (1, 2, 3, 4, 5, 6, 7)
      inspect_bitfield(0, True) -> ()
    @param[in] value
    @param[in] parity
    @param[in] size_bits = 24
    """
    result = []
    for i in range(size_bits):
        if parity:
            if value & (1 << i):
                result.append(i)
        else:
            if not value & (1 << i):
                result.append(i)
    return tuple(result)


def get_correct_addr(address, addr_per_word):
    """
    @brief For Crescendo, sometimes the address that is supplied
    does not start at the beginning of the word. Kalelfreader only returns
    values in words, therefore this function returns the value that must be subtracted
    from the address to get to the start of the word.
    @param[in] address
    @param[in] addr_per_word
    """
    if addr_per_word == 4:
        if address % addr_per_word == 0:
            return address

        value = address % addr_per_word
        return address - value

    return address


def convert_byte_len_word(length, addr_per_word):
    """
    @brief For Crescendo, if the value of a length is not divisible with the
        number of addr per word, its value must be adjusted so it will be
        divisible with the number of addr per word, in order for kalelfreader
        to return the corresponding number of words.
    @param[in] length
    @param[in] addr_per_word
    """
    div = length % addr_per_word
    if div == 0:
        return length

    return length + addr_per_word - div


def u32_to_s32(uint32):
    """
    @brief Converts an unsigned 32 bit integer to a signed 32 bit integer.
    @param[in] uint32 Unsigned 32 bit integer
    @param[ou] Signed 32 bit integer
    """
    if uint32 >= (1 << 32):
        raise Exception(
            "The value is bigger than a 32 bit unsigned integer"
        )
    if uint32 < 0:
        raise Exception("The values is a signed integer not an unsigned")
    # make the conversion and return the value.
    return uint32 - (uint32 >> 31) * (1 << 32)


def s32_to_frac32(int32):
    """
    @brief Converts an signed 32 bit integer to a signed 32 bit fractional
        value.
    @param[in] int32 Signed 32 bit integer
    @param[ou] Signed 32 bit fractional value
    """
    if int32 > ((1 << 31) - 1):
        raise Exception(
            "The value is bigger than a 32 bit signed integer"
        )
    if int32 < (-(1 << 31)):
        raise Exception(
            "The value is smaller than a 32 bit signed integer"
        )
    # make the conversion and return the value.
    return float(int32) / ((1 << 31) - 1)


def u32_to_frac32(uint32):
    """
    @brief Converts an unsigned 32 bit integer to a signed 32 bit fractional
        value.
    @param[in] uint32 Unsigned 32 bit integer
    @param[ou] Signed 32 bit fractional value
    """
    return s32_to_frac32(u32_to_s32(uint32))


def get_string_from_word(no_of_chr, word):
    """
    @brief It takes the number of characters that one wants to retrieve from a
        word and the word as an argument and turns it into a string.
    @param[in] no_of_chr
    @param[in] word
    """
    string = ""
    for _ in range(no_of_chr):
        string += chr(word & 0x000000FF)
        word = word >> 8

    return string

def uint32_array_to_uint16(unint32_array):
    """
    @brief Unpacks a 32bit array to an array of 16bit. The values of the new 16
        bit array will be calculated using the most significant 16 bit and the
        least significant 16 bit of the 32 bit value.
    @param[in] unint32_array
    @param[out] unint16_array
    """
    unint16_array = []
    for value in unint32_array:
        unint16_array.append(value & 0xffff)
        unint16_array.append((value >> 16) & 0xffff)

    return unint16_array


def uint32_array_to_uint8(unint32_array):
    """
    @brief Unpacks a 32bit array to an array of 8bit. The values of the new 8
        bit array will be calculated considering little-endian memory layout.
    @param[in] unint32_array
    @param[out] unint8_array
    """
    unint8_array = []
    for value in unint32_array:
        unint8_array.append(value & 0xff)
        unint8_array.append((value >> 8) & 0xff)
        unint8_array.append((value >> 16) & 0xff)
        unint8_array.append((value >> 24) & 0xff)

    return unint8_array

def add_indentation(input_str, nr_of_spaces):
    """
    @brief Adds identation before each line in a string.
    @param[in] nr_of_spaces Spaces before each line.
    @param[out] The formatted string.
    """
    spaces = " "*nr_of_spaces
    # add indentation for a nicer view.
    return "".join(
        spaces + line + "\n" for line in input_str.split("\n") if line != ""
    )

def mem_dict_to_string(mem_dict, words_per_line=8, compact_mode=True):
    """
    @brief Converts an ordered dictionary of memory addresses and values into
        a formatted string.
    @param[in] mem_dict Ordered dictionary of memory addresses and values.
    @param[in] words_per_line Number of words to print out. Default value
                             is 8 words.
    @param[in] compact_mode If true, removes repeating lines in the middle.
        For example:
            line1: Something 1                line1: Something 1
            line2: Something 2                line2: Something 2
            line3: Something 2                line3: *
            line4: Something 2  >conversion>  line4: Something 2
            line5: Something 2                line5: Something 3
            line6: Something 3

    """
    # Convert the buffer content to human readable addresses and values
    output_str = ""
    count = 0
    display_address = False
    for key in mem_dict:
        if isinstance(key, str):
            # String keys are reserved to mark downs
            output_str += "\n--- " + key + " at 0x%08x " % (mem_dict[key])

            # After, displaying the information resume displaying the
            # buffer content.
            display_address = True
        else:
            # display the starting address for each line.
            if count % words_per_line == 0 or display_address:
                display_address = False
                # display the new starting address
                output_str += "\n0x%08x: " % (key)
                # keep the alignment in the same way
                output_str += " " * 9 * (count % words_per_line)

            # format the value
            output_str += "%08x " % (mem_dict[key])
            count += 1

    if not compact_mode:
        return output_str

    # remove same lines to save some space at the output.
    output_str_no_duplicate = ""
    heading_length = 11
    previous_line = " " * heading_length
    same_line_count = 0
    for line in output_str.split("\n"):
        if line[heading_length:] == previous_line[heading_length:]:
            same_line_count += 1
        else:
            # display multiple copies of the same line as a * only if more
            # than 3 lines are the same.
            if same_line_count > 1:
                output_str_no_duplicate += "*\n"
            if same_line_count > 0:
                output_str_no_duplicate += previous_line + "\n"

            output_str_no_duplicate += line + "\n"
            same_line_count = 0

        previous_line = line
    # check if the the buffer ended with same values.
    if same_line_count > 1:
        output_str_no_duplicate += "*\n"
    if same_line_count > 0:
        output_str_no_duplicate += previous_line + "\n"
    return output_str_no_duplicate

def mem_size_to_string(size_in_octets, units="okw"):
    """
    @brief Returns the memory size as a formatted string in different unit.
    @param[in] size_in_octets Memory size in octets.
    @param[in] units String containing the unit to use:
        "o" - octets
        "k" - KiB
        "w" - words
    @param[out] String which contains the memory size in different
        units.
    """
    return_str = ""
    if "o" in units:
        # first format the size in octets
        return_str += "{:7,} bytes".format(size_in_octets)
    if "k" in units:
        if "o" in units:
            return_str += " ("
        return_str += "{:5.1f} KiB".format(size_in_octets / 1024.0)
        if "o" in units:
            return_str += ")"
    # if the address per words is different than one, format the size in words.
    if Arch.addr_per_word != 1 and "w" in units:
        # convert to words and round up.
        size_in_words = (size_in_octets + Arch.addr_per_word - 1) // Arch.addr_per_word
        if "o" in units or "k" in units:
            return_str += " or "
        return_str += "{:7,} words".format(size_in_words)

    return return_str

def list_to_string(in_list):
    """
    Converts a list to a string in a way to hide differences between python 3
    and 2.
    """
    # Remove 'u' which is not present in python 2.
    return builtins.str(in_list).replace(" u'", " '")

def import_module_from_path(module_name, path):
    """
    @brief Imports a python module from a given path.
    @param[in] module_name
    @param[in] path
    """
    file_handler, filename, desc = imp.find_module(module_name, [path])

    return imp.load_module(module_name, file_handler, filename, desc)


##########################################################################
# ACAT Package Helper Functions - function used to start ACAT as a package
##########################################################################

HELP_STRING = r"""
Audio Coredump Analysis Tool, v{}

Usage:
python ACAT.py [--help | -h]
    Prints this help documentation and exit.

python ACAT.py [Pythontools] [Build] [Chip] [Core] [Mode] [<other option1> ..]

Pythontools:
-t <Pythontools_path>
            Specify path to Pythontools (for kalelfreader and kalaccess).

Build:
-b <path>   Specify path to build output (inc. filename root,
            binaries, lst file, etc.). Note: This is not required if the tool
            is used internally on a released build.
-j <path>   Specify path to downloaded capability (inc. filename root,
            binaries, lst file, etc.). More than one downloaded
            capability can be specified. For every new path
            this option is required e,g "-j path1 -j path2"
-l <path>   Specify path to build output for P1 (inc. filename root,
            binaries, lst file, etc.). This option automatically enables
            dual core mode. See "-d" option for more.

Chip:
-c <path>  Specify filepath (inc. filename) of coredump to analyse
-s <transport>  Option -s allows ACAT to run on a 'live' chip rather than
            a coredump. The 'transport' parameter is a standard kalaccess
            param string that should contain the transport required
            (e.g. 'usb', 'lpt', 'kalsim', 'trb') plus a number of other
            parameters such as SPIPORT or SPIMUL.

Core:
missing option
            The default processor to analyse is 0.
-p <processor>  Specify which Kalimba processor (0, 1, ...) to debug.
-d          Required if dualcore and if both cores are using the same
            build output. In Interactive mode, to select the processor that the
            command is to be run on, it has to be called with the name of the
            processor as the instance e.g. p0.<command>, p1.<command>.

Mode:
missing option
            No option will perform Automatic mode, which attempts to perform
            analysis without prompting the user and outputting it to the
            standard output.
-i          Run in Interactive mode. Interactive mode accepts individual
            commands and will query for missing information.
-I          Same as -i, but using the IPython interpreter.
-w <path>   Filepath (inc. filename) where the html file will be created
            containing the results of the automatic analyse. (not
            compatible with interactive mode)

Example usage:
python ACAT.py -t C:\qtil\ADK_XY\tools\pythontools
    -b C:\qtil\ADK_XY\audio\kalimba\kymera\output\<conf>\build\debugbin\kymera_<chip>_audio
    -c coredump_file.xcd -d -i
python ACAT.py -t C:\qtil\ADK_XY\tools\pythontools
    -b C:\qtil\ADK_XY\audio\kalimba\kymera\output\<conf>\build\debugbin\kymera_<chip>_audio
    -s "trb/scar/0" -p 0 -w HTML_output.html
""".format(version)
#########################################

def _import_pythontools():
    """
    Function imports modules from pythontools
    """
    debug_log("pythontools_path is " + global_options.pythontools_path)
    # add pythontools to the system path.
    sys.path.append(global_options.pythontools_path)

    # Import kalelfreader for KerDebugInfo
    ker_module = import_module_from_path(
        'kalelfreader_lib_wrappers', global_options.pythontools_path
    )
    global_options.ker = ker_module

    # Import kalmemaccessors for LiveSPI amd LiveKalcmd
    kmem = import_module_from_path(
        'kalmemaccessors', global_options.pythontools_path
    )
    global_options.kmem = kmem

    if global_options.live:
        # Import kalaccess for LiveSpi
        kalaccess_module = import_module_from_path(
            'kalaccess', global_options.pythontools_path
        )
        global_options.kal = kalaccess_module.Kalaccess()
        if (global_options.is_dualcore) or (global_options.processor == 1):
            global_options.kal2 = kalaccess_module.Kalaccess()

    debug_log("coredump_path is " + global_options.coredump_path)
    if global_options.live:
        if global_options.kalcmd_object is not None:
            debug_log("Running on a live chip over kalcmd2")
        else:
            debug_log("Running on a live chip")

# Reading options needs a lot of branches so disable pylint for this for loop
def parse_args(parameters):  # pylint: disable=too-many-branches,too-many-statements
    """
    @brief Function which parses the input parameters of ACAT.
    @param[in] parameters Input parameters.
    """
    import ACAT.Core.CoreTypes as ct
    # Assign the command-line options
    try:
        options, args = getopt.getopt(
            parameters, "b:c:w:k:l:t:hiImdp:s:a:j:q",
            ["help", "elf_check", "patch="]
        )
    except getopt.GetoptError as err:
        raise ct.Usage("ERROR: " + builtins.str(err))

    for option, value in options:
        if option in ("-h", "--help"):
            print(HELP_STRING)
            sys.exit()
        elif option == "-b":
            global_options.build_output_path_p0 = os.path.normcase(value)
            if global_options.build_output_path_p1 == "":
                # in case the dualcore image is using a single build output
                global_options.build_output_path_p1 = global_options.build_output_path_p0
        elif option == "-c":
            global_options.coredump_path = os.path.normcase(value)
            # Increase the cache validity internal because all the data is
            # static. 10 minutes will be probably enough to hold the data
            # for the whole automatic run.
            global_options.cache_validity_interval = 600.0
        elif option == "-w":
            global_options.html_path = os.path.normcase(value)
        elif option == "-k":
            if global_options.pythontools_path == "":
                kalimbalab_path = os.path.abspath(value)
                global_options.pythontools_path = kalimbalab_path + \
                    os.path.normcase("/pythontools")
            else:
                raise ct.Usage("'-k' option conflicting '-t' option")
        elif option == "-t":
            if global_options.pythontools_path == "":
                global_options.pythontools_path = os.path.abspath(value)
            else:
                raise ct.Usage("'-k' option conflicting '-t' option")
        elif option == "-i":
            global_options.interactive = True
        elif option == "-I":
            global_options.interactive = True
            global_options.use_ipython = True
        elif option == "-m":
            global_options.build_mismatch_allowed = True
        elif option == "-p":
            global_options.processor = int(value)
        elif option == "-s":
            global_options.live = True
            global_options.spi_trans = value
        elif option == "-d":
            if global_options.kalcmd_object is not None:
                raise ct.Usage("Kalcmd does not support dual core")
            else:
                global_options.is_dualcore = True
        elif option == "-l":
            if global_options.kalcmd_object is not None:
                raise ct.Usage("Kalcmd does not support dual core")
            else:
                global_options.is_dualcore = True
                global_options.build_output_path_p1 = os.path.normcase(value)
        elif option == "-a":
            # Internal option not advertised to users: -a <sim_object>
            # Kalcmd2 object used to connect to a simulation without
            # enabling debug mode.
            if global_options.is_dualcore:
                raise ct.Usage("Kalcmd does not support dual core")
            else:
                global_options.kalcmd_object = value
                global_options.live = True
        elif option == '-j':
            if global_options.bundle_paths is None:
                global_options.bundle_paths = []
            global_options.bundle_paths.append(value)
        elif option == '-q':
            # Thi flag is only used when ACAT is launched from QDME. When this
            # flag is set ACAT will wait until the chip is booted.
            global_options.wait_for_proc_to_start = True
        elif option == "--elf_check":
            global_options.dependency_check = True
        elif option == "--patch":
            global_options.patch = value
        else:
            raise ct.Usage("ERROR: Unhandled option")

    if args:
        raise ct.Usage("ERROR: Unknown option " + builtins.str(args))

    # Output error if we're missing anything essential.
    if (not global_options.live) and (global_options.coredump_path == ""):
        raise ct.Usage("ERROR: Path to coredump file not supplied")
    if not os.path.exists(global_options.pythontools_path):
        raise ct.Usage(
            "ERROR: Path to Pythontools %s invalid or not supplied" %
            global_options.pythontools_path)

    _import_pythontools()


#########################################


def common_func(proc):
    """
    @brief This function contains functionalities that are applied to each
        processor instance that does special decoration for pciespi.
    @param[in] proc Proccessor object
    """
    if "pciespi" in global_options.spi_trans:
        # pciespi transport doesn't read good values until the clock is up to the speed in the DUT.
        # The reading from the dm memory will be decorated to read the first element
        # from the capability data table until is null. ( The first element from the
        # capability tables  is the first capability address. This table is null
        # terminated and we know that at least one capability is installed)
        # Because the read method is static we need to decorate the class.
        var = proc.debuginfo.get_var_strict("$_capability_data_table")
        address = var.address

        def _dm_decorator(getitem, address):
            # function decorator
            def _new_getitem(self, index):
                prev_value = getitem(self, address)
                value = getitem(self, address)
                while (prev_value != value) or (value == 0):
                    prev_value = value
                    value = getitem(self, address)
                return getitem(self, index)

            return _new_getitem

        def _dm_accessor_decorator(kls):
            # class decorator
            kls.__getitem__ = _dm_decorator(kls.__getitem__, address)
            return kls

        dm_accessor_mod = _dm_accessor_decorator(global_options.kmem.DmAccessor)
        if proc.processor == 0:
            global_options.kal.dm = dm_accessor_mod(global_options.kal)
        else:
            global_options.kal2.dm = dm_accessor_mod(global_options.kal2)
        print("kalmemaccessors.DmAccessor was decorated for pciespi")


#########################################


def load_session(analyses=None):
    """
    @brief Returns an interpreter loaded with the specified analyses.
    @param[in] analyses List if analyses to load. If none all the existent
                        analyses will be loaded.
    """
    # import here to avoid circular dependency.
    from ..Core.MulticoreSupport import Processor
    from ..Interpreter.Interactive import Interactive
    from ..Interpreter.Automatic import Automatic
    from ..Display.PlainTextFormatter import PlainTextFormatter
    from ..Display.HtmlFormatter import HtmlFormatter
    from ..Display.InteractiveFormatter import InteractiveFormatter

    # In non-interative mode we can use "-" to indicate that the coredump
    # is being supplied on the standard input. Since we need to parse it
    # more than once, we slurp it into an array here so we can iterate over
    # it later. Because this feature was quite a late addition, we signal
    # what's going on by putting the array into coredump_path and detecting
    # it's an array later. This is ugly.
    if global_options.coredump_path == '-':
        global_options.coredump_path = sys.stdin.readlines()

    # decide on the formatter
    if global_options.interactive:
        formatter = InteractiveFormatter()
    else:
        if global_options.html_path == "":
            formatter = PlainTextFormatter()
        else:
            formatter = HtmlFormatter(global_options.html_path)

    # create the processors
    if global_options.processor == 0:
        p0 = Processor(
            global_options.coredump_path, global_options.build_output_path_p0,
            0, formatter
        )
        debug_log(
            "build_output_path for P0 is " +
            global_options.build_output_path_p0
        )
        kwargs = {'p0': p0}
        common_func(p0)

        if global_options.is_dualcore:
            p1 = Processor(
                global_options.coredump_path,
                global_options.build_output_path_p1, 1, formatter
            )
            kwargs['p1'] = p1
            debug_log(
                "build_output_path for P1 is " +
                global_options.build_output_path_p1
            )
    else:
        p1 = Processor(
            global_options.coredump_path, global_options.build_output_path_p1,
            1, formatter
        )
        debug_log(
            "build_output_path for P1 is " +
            global_options.build_output_path_p1
        )
        kwargs = {'p1': p1}
        common_func(p1)

        if global_options.is_dualcore:
            p0 = Processor(
                global_options.coredump_path,
                global_options.build_output_path_p0, 0, formatter
            )
            debug_log(
                "build_output_path for P0 is " +
                global_options.build_output_path_p0
            )
            kwargs = {'p0': p0}
            common_func(p0)

    # if analyses is not none only the given analysis will be run.
    if analyses:
        kwargs['analyses'] = analyses

    # return the formatter
    if global_options.interactive:
        return Interactive(**kwargs)
    return Automatic(**kwargs)


#########################################
def set_matplotlib_plotter(plotter_dict):
    """
    Sets the matplotlib plotter.Function used for test purposes.
    """
    global_options.plotter_dict = plotter_dict

def create_matplotlib_plotter():
    """
    Creates a dictionary which contains necessary information to manipulate a
    matplotlib plotter. Function used for test purposes.
    """
    # create the input and output queues, the lock which is used to make sure
    # only one thread is using the plotter and the thread itself.
    input_q = Queue()
    output_q = Queue()
    plotter_obj = MatplotlibPlotter(input_q, output_q)
    lock = threading.Lock()
    # start the thread
    plotter_obj.start()
    # Put all the necessary information to a dict.
    global_options.plotter_dict = {
        "input":input_q,
        "output":output_q,
        "plotter":plotter_obj,
        "lock":lock
    }
    return global_options.plotter_dict

def destroy_matplotlib_plotter():
    """
    Terminates and destroys the matplotlib plotter.
    Function used for test purposes.
    """
    # Terminate the plotter thread by sending None as input.
    global_options.plotter_dict["input"].put(None)
    # for the task to finish.
    global_options.plotter_dict["plotter"].join()

def do_analysis(session):
    """
    Runs all the available analyses.
    """
    # When testing ACAT the plotter is provided by the test system.
    if not global_options.under_test:
        matplotlib_plotter = create_matplotlib_plotter()
        set_matplotlib_plotter(matplotlib_plotter)
    try:
        session.start()  # Do as much as we can, then leave
        session.join()  # wait for the thread to finish
    except (KeyboardInterrupt, SystemExit):
        print("Keyboard interrupt!")

    if not global_options.under_test:
        destroy_matplotlib_plotter()
        os._exit(0)
