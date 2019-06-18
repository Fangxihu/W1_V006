############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module used to read a coredump
"""
import sys
import copy
import itertools
import re
import os
try:
    # Python 2
    from itertools import imap
except ImportError:
    # Python 3
    imap=map

# pylint: disable=wrong-import-position
if __name__ == '__main__':
    # Remove the directory of this file from the sys.path and replace it
    # with the parent directory to allow importing ACAT as a package
    FILE_DIR = os.path.abspath(os.path.dirname(__file__))
    try:
        # remove all the occurences
        while True:
            sys.path.remove(FILE_DIR)
    except ValueError:
        pass
    ACAT_DIR = os.path.abspath(os.path.join(FILE_DIR, os.pardir))
    ACAT_DIR = os.path.abspath(os.path.join(ACAT_DIR, os.pardir))
    print("ACAT directory: %s" % ACAT_DIR)
    sys.path.append(ACAT_DIR)

from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from ACAT.Interpreter import Interactive as ia
from ACAT.Core import Arch
from ACAT.Core import ChipData


class BankedReg(object):
    """
    @brief Provides a clear structure for storing banked registers.
    """

    def __init__(self, addr_bank_sel_reg, bank, addr):
        """
        @brief Initialise the specific information for a banked register.
        @param[in] self Pointer to the current object
        @param[in] addr_bank_sel_reg
        @param[in] bank
        @param[in] addr Specific address of a register in the banks
        """
        self.addr_bank_sel_reg = addr_bank_sel_reg
        self.bank = bank
        self.addr = addr
        self.value = None


##################################################


class Coredump(ChipData.ChipData):
    """
    @brief Provides access to chip data stored in a coredump
    """

    def __init__(self, coredump_filepath, processor):
        """
        @brief Load a coredump, and parse it into sections
        @param[in] self Pointer to the current object
        @param[in] coredump_filepath
        @param[in] processor
        """
        ChipData.ChipData.__init__(self)

        self.coredump_filepath = coredump_filepath
        self.chip_id = None  # Global chip version (e.g. 12280000)
        # string containing one of "Bluecore", "Hydra", "KAS".
        self.chip_architecture = None
        self.kalimba_architecture = None  # integer, e.g. 3, 4, 5.
        self.chip_revision = None
        # the coredump must be read twice from each processors' perspective
        self.processor = processor

        self.firmware_id = 0  # Firmware ID integer.
        self.firmware_id_string = ""  # Firmware ID string.
        # Dictionary that will contain PM RAM (if it is tagged as 'PM' in the
        # coredump)
        self.pm = {}
        # Dictionary that will contain all of the data from the coredump
        self.data = {}
        # (stuff marked as 'data' or memory-mapped registers; this could
        # be DM-mapped PM in the case of HydraCore chips).
        self.registers = {}  # Dictionary that holds all processor registers
        self.banked_registers = []
        # addr_per_word is 1 for all the supported chips, except Crescendo
        self.addr_per_word = 1
        self.is_banked_register = False
        self.ignore_processor = False
        # parse the coredump file
        # We deal with reading from stdin by saving the lines into an array
        # once during startup and replacing the filename with the array. This
        # is ugly but it was a very late feature.
        if isinstance(self.coredump_filepath, list):
            self._read_xcd_file(
                imap(lambda x: x.rstrip(), self.coredump_filepath)
                .__iter__()
            )
        else:
            with open(self.coredump_filepath) as coredump_file:
                self._read_xcd_file(
                    imap(lambda x: x.rstrip(), coredump_file)
                    .__iter__()
                )

        # Finished parsing the audio section. Set the chip architecture for the rest
        # of the tool.
        Arch.chip_select(
            self.kalimba_architecture, self.chip_architecture, self.chip_id,
            self.chip_revision
        )

        # Now we do some complicated things to accommodate
        # architecture-specific stuff.

        if (
                self.kalimba_architecture == 4 or self.kalimba_architecture == 5
            ) and self.chip_architecture == "Hydra":
            # The DM1 RAM range is also aliased at the DM2 range
            # Only the DM1 data is in the coredump, so make a copy for easy
            # analysis
            dm2 = {}
            for addr in self.data:
                if Arch.get_dm_region(addr) == "DM1RAM":
                    dm2[addr + Arch.dRegions['DM2RAM'][0]] = self.data[addr]
            self.data.update(dm2)

        cu.debug_log('Coredump parsed')

    def _read_xcd_file(self, coredump_iterator):
        # Read the File signature (XCD2, XCD3)
        line = next(coredump_iterator)

        if 'XCD3' in line:
            # A HydraCore coredump, e.g. Amber.
            self.chip_architecture = "Hydra"
            # We use the 'SS' directive to find each subsystem
            section_directive = r'SS '
            section_audio = 'SS AUDIO'
        else:
            # A non-HydraCore coredump, e.g. Gordon
            self.chip_architecture = "Bluecore"
            # Use the 'P' directive to find each processor
            section_directive = r'P '
            section_audio = 'P DSP'

        # Parse the general (non-subsystem-specific) bit
        # of coredump data for things like the global chip
        # version. This includes everything we see prior
        #  to the first section directive.
        while line[:len(section_directive)] != section_directive:
            try:
                line = next(coredump_iterator)
            except StopIteration:
                # Hit the end of the file
                raise ct.CoredumpParsingError(
                    "No DSP section found - don't forget to use "
                    "the '-dsp' option when running coredump"
                )
            if line[:3] == 'AT ':
                # AT: architecture type, e.g. "AT BC7"
                # Don't know whether this is useful or not.
                pass
            elif line[:3] == 'AV ':
                # AV: architecture version, e.g. "AV 12280000"
                # First 16 bits is split into {revision[15:8], chip_id[7:0])
                # Second 16 bits is meaningless on any chip we care about (BC7
                # or later).
                self.chip_id = (int(line[3:], 16) & 0x00ff0000) >> 16
                self.chip_revision = (int(line[3:], 16) & 0xff000000) >> 24
            else:
                # Something else we don't care about
                pass

        try:
            # Now seek to the Audio Subsystem's bit within this coredump
            while section_audio not in line:
                line = next(coredump_iterator)
        except StopIteration:
            # Hit the end of the file
            raise ct.CoredumpParsingError("No Audio Subsystem found!")

        # Now go through the audio section and look for
        # XCD2 directives. Read everything
        # until either the start of the next section,
        # or the end of the file (whichever comes first).
        # NB the space in the directive string is important. For example, a
        # line of data could start 'DD'.

        # These variables keep track of which data block we're currently
        # parsing (PM, DM, memory-mapped registers, ...)
        datablock = None
        addr = None
        extent = None
        bank_reg = None
        bank = None
        addr_bank_sel = None

        # There is also a rule that data entries have
        # to be at least 4 hex-digits long, which means
        # the natural way of dumping data on a 32-bit
        # platform (one octet at a time) is forbidden.
        # (See the 'line of data values' section)
        for line in coredump_iterator:
            if line[:len(section_directive)] != section_directive:
                if line[:3] == 'AT ':
                    # AT: architecture type, e.g. "AT KALIMBA5"
                    match = re.search(r'KALIMBA(\d)', line)
                    if match is not None:
                        self.kalimba_architecture = int(match.groups()[0])
                        # this will need to be removed after the changes in
                        # B-212528 take place.
                        if self.kalimba_architecture == 4:
                            self.addr_per_word = 4
                    datablock = None
                elif line[:3] == 'AV ':
                    # AV: chip ID, e.g. "AV 01f5"
                    self.dsp_version = int(line[3:], 16)
                    datablock = None
                elif line[:3] == 'CM ':
                    # CM: comment. Ignore.
                    datablock = None
                elif line[:3] == 'II ' and not self.ignore_processor:
                    # II: build ID (integer), e.g. "II FFFF"
                    # Not available in old Bluecore coredumps (before
                    # B-204537).
                    self.firmware_id = int(line[3:], 16)
                    datablock = None
                elif line[:3] == 'IS ' and not self.ignore_processor:
                    # II: build ID string, e.g. "IS UNRELEASED
                    # amber_lpc svc-audio_dsp 2012-06-08 16:27:34"
                    # Not available in old Bluecore coredumps (before B-204537).
                    # Remove newline
                    self.firmware_id_string = line[3:].rstrip()
                    if self.firmware_id_string[0] == "\"":
                        self.firmware_id_string = self.firmware_id_string[1:-1]
                    datablock = None
                elif line[:2] == 'R ' and not self.ignore_processor:
                    # R: processor register, e.g. "R PC 41065B"
                    # Store the register. Prefix register name with 'REGFILE_'
                    # to match the symbol table/io_map.asm
                    words = line.split()
                    regname = "REGFILE_" + words[1].upper()
                    # integer value
                    self.registers[regname] = int(words[2], 16)
                    datablock = None
                elif line[:3] == 'RR ' and not self.ignore_processor:
                    # RR: memory mapped register, e.g. "RR 00FFFF8C 00000001"
                    # Store the register.
                    words = line.split()
                    regname = "0x" + words[1][2:]  # ignore the first byte
                    # integer value
                    self.registers[regname] = int(words[2], 16)
                    datablock = None
                elif line[:3] == 'DC ':
                    # DC: PM RAM section, e.g. "DC 00000000 00003000". Values
                    # are {start address, length}
                    interval = line.split()
                    addr = int(interval[1], 16)  # integer address
                    extent = addr + int(interval[2], 16)
                    datablock = self.pm
                elif line[:3] == 'DD ':
                    # DD: DM RAM section, e.g. "DD 00000000 00004000". Values
                    # are {start address, length}
                    interval = line.split()
                    addr = int(interval[1], 16)  # integer address
                    extent = addr + int(interval[2], 16)
                    datablock = self.data
                elif line[:3] == 'DR ' and self.is_banked_register:
                    interval = line.split()
                    addr = int(interval[1], 16)
                    extent = int(interval[2], 16)
                    if addr_bank_sel is None:
                        raise ct.CoredumpParsingError((
                            "Banked register (BR) statements must be "
                            "preceded by bank select (BS) statement."
                        ))
                    bank_reg = BankedReg(addr_bank_sel, bank, addr)
                    datablock = None
                elif line[:3] == 'DR ' and not self.is_banked_register:
                    # DR: section of memory-mapped registers, e.g. "DR 00FFFE00
                    # 00000100". Values are {start address, length}
                    interval = line.split()
                    addr = int(interval[1], 16)  # integer address
                    extent = addr + int(interval[2], 16)
                    # We don't have a separate section for memory-mapped
                    # registers; put them in 'data'.
                    datablock = self.data
                elif line[:3] == 'BS ' and not self.ignore_processor:
                    # The BS directive indicates the start of the dumping of
                    # banked registers. e. g. BS FFFF8E64 00000003. Values are
                    # (start address, current_value).
                    addr_bank_sel = int(line.split()[1], 16)
                    self.is_banked_register = True
                elif line == 'BE':
                    # BE directive acts as brackets enclosing a set of banked
                    # register output.
                    self.is_banked_register = False
                elif line[:3] == 'RW ' and not self.ignore_processor:
                    # RW: Selects the bank and modifies the banked register.
                    # Not Implemented yet.
                    contents = line.split()
                    bank = int(contents[5], 16)
                elif line[:3] == 'P 0':
                    # The data collected from the P0 part goes into self.data
                    # (when read from P0's perspective)
                    if self.processor == 0:
                        datablock = self.data
                        self.ignore_processor = False
                    else:
                        self.ignore_processor = True
                elif line[:3] == 'P 1':
                    # The data collected from the P1 part goes into self.data
                    # (when read from P1's perspective)
                    if self.processor == 1:
                        datablock = self.data
                        self.ignore_processor = False
                    else:
                        self.ignore_processor = True

                elif self.ignore_processor:
                    continue
                elif datablock is not None and \
                    (re.match(r'[0-9A-Fa-f]{4,}', line) != None) and \
                    not self.ignore_processor:
                    # It's a line of data values. Data entries are in hex,
                    # and need to have at least 16 bits.
                    # Add them to whichever block we're currently parsing.
                    values = line.split()
                    for value in values:
                        # It would be nice if we had one value per address,
                        # wouldn't it? Sadly that can't always happen; data
                        # values in an xcd have to be at least 16 bits,
                        # so if DM is 8-bit addressed we have to dump it
                        # a word at a time. That means we need to check
                        # the architecture type to work out whether the
                        # value we have here corresponds to a single address
                        # (24-bit platforms) or four addresses (32-bit
                        # platforms). If we haven't got an AT directive by
                        # this point then we're in a bad situation.
                        # (We could store everything as if it were
                        # 24-bit, then try and correct it
                        # later, but it is not worth it.)
                        if self.kalimba_architecture is None:
                            raise ct.CoredumpParsingError(
                                "No AT directive found prior to data section")

                        # Pylint error due to known pylint bug
                        # https://www.logilab.org/ticket/3207
                        # pylint: disable=unsupported-assignment-operation
                        datablock[addr] = int(value, 16)
                        # pylint: enable=unsupported-assignment-operation
                        addr += self.addr_per_word
                        # Safety check
                        if addr > self.addr_per_word * extent:
                            raise ct.CoredumpParsingError(
                                "Data value out of interval: " +
                                cu.hex(addr) + " > " + cu.hex(extent)
                            )

                elif self.is_banked_register and (
                        bank_reg is not None
                    ):
                    values = line.split()
                    if len(values) == 1:
                        bank_reg.value = int(values[0], 16)
                        self.banked_registers.append(bank_reg)
                    else:
                        raise ct.CoredumpParsingError(
                            "Data value out of interval at address " +
                            cu.hex(bank_reg.addr) + ' bank ' + cu.hex(bank_reg.bank))

                else:
                    # Something unexpected. Raise an exception so that we can
                    # fix this parser.
                    raise ct.CoredumpParsingError(
                        "Unrecognised or unsupported coredump formatting")
                # end of if elif else
            else:
                # if there is any SS following the audio one, ignore it
                break
            # end of section
        # end of for
        # Check if we found any useful data at all
        if self.data == {}:
            raise ct.CoredumpParsingError(
                "The audio subsystem (SS AUDIO) contains no data!")

    ############################
    def is_volatile(self):
        """
        @brief Allows the user to query whether the data is volatile (e.g. from a
        live chip) or fixed (e.g. from a coredump).
        @param[in] self Pointer to the current object
        """
        return False

    def set_data(self, address, value):
        """
        @brief Sets the contents is not possible for a coredump
        @param[in] address
        @param[in] value
        """
        raise Exception("Cannot set data for coredump.")

    ############################
    # default length of 0 is special; see below
    def get_data(self, address, length=0, ignore_overflow=False):
        """
        @brief Returns the contents of one or more addresses in DM.
        This allows you to grab a chunk of memory, e.g. get_data(0x600, 50).
        Note:
         The length supplied is measured in addressable units.

         get_data(addr) will return a single value;
         get_data(addr, 1) will return a tuple with a single member.
         get_data(addr, 10) will return a tuple with ten members or a tuple with
         three members (when the memory is octet addressed, 32bit words).

        If the address is out of range, a KeyError exception will be raised.
        If the address is valid but the length is not (i.e. address+length
        is not a valid address) an OutOfRange exception will be raised.
        @param[in] self Pointer to the current object
        @param[in] address
        @param[in] length = 0
        @param[in] ignore_overflow Ignore if the read goes out from the memory
            region and append zeros. This is useful if an union is at the end
            of the memory region.
        """
        # We helpfully accept address as either a hex string or an integer
        try:
            # Try to convert address from a hex string to an integer
            address = int(address, 16)
        except TypeError:
            # Probably means address was already an integer
            pass
        # if the address does not start at the beginning of a word, correct it
        address = cu.get_correct_addr(address, Arch.addr_per_word)
        # By default, return a single value.
        if length == 0:
            return self.data[address]

        # Otherwise, return a list.
        # This method deliberately copies from self.data, rather than returning
        # a reference to our 'master copy'. We don't want a caller modifying
        # that.
        rawdata = []
        # length needs to have a value that can fetch words;
        # if it does not, adjust it by adding
        # the missing number of bytes
        length = cu.convert_byte_len_word(length, Arch.addr_per_word)
        for i in range(0, length, Arch.addr_per_word):
            if address + i in self.data:
                rawdata.append(self.data[address + i])
            else:
                # We want to raise a different exception depending on whether
                # address is valid or not. Unfortunately this requires some
                # special treatment for i == 0
                if i == 0:
                    raise ct.InvalidDmAddress(
                        "No DM at " + hex(address)
                    )
                if ignore_overflow:
                    # Just append zeros if we running out of the memory.
                    rawdata.append(0)
                else:
                    raise ct.InvalidDmLength(i, address)
        return tuple(rawdata)

    def get_data_pm(self, address, length=0):
        """
        @brief This method works in the exactly the same way as
            get_data(self, address, length = 0) method, but it returns data from
            PM region instead of DM.
        @param[in] self Pointer to the current object
        @param[in] address
        @param[in] length = 0
        """
        try:
            address = int(address, 16)
        except TypeError:
            pass
        address = cu.get_correct_addr(address, Arch.addr_per_word)
        if length == 0:
            return self.pm[address]

        # note: Even the PM RAM is saved in self.pm data section.
        rawdata = []
        length = cu.convert_byte_len_word(length, Arch.addr_per_word)
        for i in range(0, length, Arch.addr_per_word):
            if address + i in self.pm:
                rawdata.append(self.pm[address + i])
            else:
                if i == 0:
                    raise KeyError(
                        "Key " +
                        cu.hex(
                            address +
                            i) +
                        " is invalid")
                else:
                    raise ct.OutOfRange(
                        "Key " + cu.hex(address + i) + " is invalid",
                        address + i - Arch.addr_per_word)
        return tuple(rawdata)

    def get_proc_reg(self, name):
        """
        @brief Return the value of the processor register specified in 'name'.
        name is a string containing the name of the register in upper or
        lower case, with or without the prefix 'REGFILE_'
        e.g. "REGFILE_PC", "rMAC", "R10".
        This may raise a number of exceptions, such as KeyError or
        AttributeError.
        @param[in] self Pointer to the current object
        @param[in] name
        """
        if name.upper() in self.registers:
            return self.registers[name.upper()]
        elif 'REGFILE_' + name.upper() in self.registers:
            return self.registers[('REGFILE_' + name.upper())]

        raise KeyError("Register name " + name + " is invalid")

    def get_all_proc_regs(self):
        """
        @brief Returns a dictionary containing all processor registers.
        @param[in] self Pointer to the current object
        """
        # Don't give the caller the original data; we don't want to risk
        # them corrupting it.
        regs_copy = copy.deepcopy(self.registers)
        return regs_copy

    def get_banked_reg(self, address, bank=None):
        """
        @brief Return the value of a banked register in a specific
            bank (default is the current value in the register).
        @param[in] self Pointer to the current object.
        @param[in] address Address of the banked register.
        @param[in] bank = None
        """
        if bank is not None:
            for member in self.banked_registers:
                if member.addr == address and member.bank == bank:
                    return member.value
            raise KeyError("Invalid register address " +
                           cu.hex(address) + " or bank " + bank)
        else:
            for member in self.banked_registers:
                if member.addr == address:
                    return self.get_data(address)
            raise KeyError(
                "Invalid banked register address " +
                cu.hex(address))

    def get_all_banked_regs(self, addr_bank_sel_reg):
        """
        @brief Returns a string containing all banks in a
            banked register given the address of the register
            used to select the bank.
        @param[in] self Pointer to the current object.
        @param[in] addr_bank_sel_reg The address of the bank select register
            that controls the bank.
        """
        banks = ''
        is_valid_bank_reg = False
        banks += 'Banks for BSR at address ' + \
            str(cu.hex(addr_bank_sel_reg)) + ':\n'
        for member in self.banked_registers:
            if member.addr_bank_sel_reg == addr_bank_sel_reg:
                is_valid_bank_reg = True
                banks += 'Bank ' + str(member.bank) + ' address ' + str(
                    cu.hex(member.addr)
                ) + ' value ' + str(cu.hex(member.value)) + '.\n'
        if not is_valid_bank_reg:
            raise KeyError("Invalid address of a bank select register " +
                           cu.hex(addr_bank_sel_reg))

        return banks

    def get_firmware_id(self):
        """
        @brief Returns the firmware ID integer.
        @param[in] self Pointer to the current object
        """
        return self.firmware_id

    def get_firmware_id_string(self):
        """
        @brief Returns the firmware ID string.
        @param[in] self Pointer to the current object
        """
        return self.firmware_id_string.strip(
        )  # Strip any leading/trailing whitespace


HELP_STRING = """
    Standalone coredump parser

    Usage
        python Coredump.py <path>

    Parameters
        <path>       Specify filepath (inc. filename) of coredump to analyse
    """

INTERACTIVE_HELP_STRING = """
Coredump is loaded as variable 'cd'.
Example commands:
    help(cd)
    print cd.get_firmware_id()
    a = cd.get_data(0xf00, 0x45)

help(foo) calls the standard help routine for thing 'foo'.
"""

def main():
    """
    Main function used to run the coredump alone.
    """
    if len(sys.argv) != 2:
        sys.stderr.write(HELP_STRING)
        return 2

    user_variables = {}
    coredump_path = sys.argv[1]
    print("Coredump path is %s" % coredump_path)
    coredump = Coredump(coredump_path, 0)
    user_variables["coredump"] = coredump
    user_variables["cd"] = coredump

    history_console = ia.HistoryConsole(local_vars=user_variables)
    # enter to interactive mode
    history_console.interact(INTERACTIVE_HELP_STRING)

    return 0  # exit errorlessly



if __name__ == '__main__':
    # Perhaps we want to analyse a coredump without any symbols.
    sys.exit(main())
