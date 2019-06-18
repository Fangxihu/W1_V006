############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
This module is used to hold all the clobal types in ACAT.
"""
import string

from ACAT.Core import CoreUtils as cu

#############
# Exceptions
#############


class Usage(Exception):
    """
    @brief Exception: Something has been called in the wrong way.
    """
    pass


class FormatterError(Exception):
    """
    @brief Exception: User to signal a formatter error.
    """
    pass

class AnalysisError(Exception):
    """
    @brief Exception:
        An error occured; we can't complete this analysis.
        When raising the exception, we can choose to pass a simple string
        argument(as normal for exceptions), or pass a tuple of
        (string, argument).
    """

    def __str__(self):
        if len(self.args) == 1:
            apology = self.args[0]
        else:
            apology = self.args[0] + " " + str(self.args[1])
        return apology


class FatalAnalysisError(Exception):
    """
    @brief Exception:
        A fatal error occured; we should stop all analysis and bail out!
        (This is drastic: only use if we really can't do *any* work at all.)
    """
    pass


class OutdatedFwAnalysisError(Exception):
    """
    @brief Exception: The firmware is outdated compared to the analyses.
    """
    pass


class CoredumpParsingError(Exception):
    """
    @brief Exception: An error occurred while parsing the coredump
    """
    pass


class OutOfRange(KeyError):
    """
    @brief Exception:
        The supplied address was valid, but the range was not.
        When raising the exception, we pass a tuple of
        (string, last_valid_key).
    """

    def __str__(self):
        return self.args[0] + "; last valid key was: " + cu.hex(self.args[1])


class ChipNotPowered(Exception):
    """
    @brief Exception:
        The connection to the Live chip appears to have been lost.
    """
    pass


class AmbiguousSymbol(Exception):
    """
    @brief Exception:
        Multiple symbol matches were found and couldn't decide which to return.
        When raising the exception, we can choose to pass a simple string
        argument(as normal for exceptions), or pass a tuple of
        (string, args_list).
    """

    def __str__(self):
        if len(self.args) == 1:
            apology = self.args[0]
        else:
            try:
                output_string = ""
                # Sort matches by name
                sorted_match = sorted(
                    self.args[1],
                    key=lambda match: match["name"]
                )
                for match in sorted_match:
                    output_string += match["name"] + \
                        "\n\tElf id:" + cu.hex(match["elf_id"])

                    # Do not print out file paths when under testing because
                    # the path is environment dependent.
                    if cu.global_options.under_test:
                        output_string += "\n"
                    else:
                        output_string += "\n\tFile:" + match["file_path"] + "\n"
                apology = self.args[0] + "\n" + output_string
            except TypeError:
                apology = self.args[0] + "  " + str(self.args[1])

        return apology


class BundleMissingError(Exception):
    """
    @brief Exception:
        A downloadable bundle elf is missing.
    """
    pass


class DebugAndChipdataError(Exception):
    """
    @brief Exception:
        General exceptions for debug information and chipdata.
    """
    pass


class ChipdataError(DebugAndChipdataError):
    """
    @brief Exception:
        General exceptions for chipdata errors.
    """
    pass


class DebugInfoError(DebugAndChipdataError):
    """
    @brief Exception:
        General exceptions for debug information
    """
    pass


class DebugInfoNoVariable(DebugInfoError):
    """
    @brief Exception:
        Variable not found in debug info.
    """
    pass


class DebugInfoNoLabel(DebugInfoError):
    """
    @brief Exception:
        Code label not found in debug info.
    """
    pass

class InvalidPmAddress(DebugInfoError):
    """
    @brief Exception:
        The address is an invalid PM address.
    """
    pass

class UnknownPmEncoding(DebugInfoError):
    """
    @brief Exception:
        Unknown PM encoding for the address.
    """
    pass


class InvalidDebuginfoCall(DebugInfoError):
    """
    @brief Exception:
        DebugInfo received an invalid call.
    """
    pass


class InvalidDebuginfoType(DebugInfoError):
    """
    @brief Exception:
        DebugInfo received an invalid call.
    """
    pass


class InvalidDebuginfoEnum(DebugInfoError):
    """
    @brief Exception:
        DebugInfo received an invalid call.
    """
    pass



class InvalidDmConstAddress(DebugInfoError):
    """
    @brief Exception:
        The address is an invalid DM constant address.
    """
    pass

class InvalidDmConstLength(DebugInfoError):
    """
    @brief Exception:
        The lengh of the DM constant read is invalid.
    """
    def __init__(self, max_length, address):
        DebugInfoError.__init__(self)
        self.max_length = max_length
        self.address = address

    def __str__(self, *args, **kwargs):
        return (
            "Can only read " + cu.hex(self.max_length) + "words at address " +
            cu.hex(self.address)
        )

class InvalidDmAddress(ChipdataError):
    """
    @brief Exception:
        The address is an invalid DM address.
    """
    pass

class InvalidDmLength(ChipdataError):
    """
    @brief Exception:
        The lengh of the DM read is invalid.
    """
    def __init__(self, max_length, address):
        ChipdataError.__init__(self)
        self.max_length = max_length
        self.address = address

    def __str__(self, *args, **kwargs):
        return (
            "Can only read " + cu.hex(self.max_length) + "words at address " +
            cu.hex(self.address)
        )

#############
# Data types
#############


class ConstSym(object):
    """
    @brief A symbolic constant, e.g. $sbc.mem.ENC_SETTING_BITPOOL_FIELD or
    $_REGFILE_PC.
    Note that register names are constants, and their values are the address
    of the register. (If the contents of register is also known then it should
    be wrapped as a DataSym instead.)
    """

    def __init__(self, name, value):
        self.name = name
        self.value = value

    def __str__(self):
        return (
            'Name: ' + str(self.name) + '\n' + 'Value: ' + cu.hex(self.value) +
            '\n'
        )


class DataSym(object):
    """
    @brief A data symbol (register, dm_const entry, variable).
    """

    def __init__(self, name, address, value=None):
        self.name = name
        self.address = address  # Stored as an integer
        self.value = value  # Integer; won't necessarily know this.

    def __str__(self):
        # Special rule since you can't call hex() on None
        if self.address is not None:
            hex_addr = cu.hex(self.address)
        else:
            hex_addr = 'None'

        if self.value is not None:
            hex_val = cu.hex(self.value)
        else:
            hex_val = 'None'

        # Using str(x) here since it copes with the value being None
        return ('Name: ' + str(self.name) + '\n' + 'Address: ' +
                hex_addr + '\n' + 'Value: ' + hex_val + '\n')


class Variable(DataSym):
    """
    @brief A variable (extends DataSym). Variables are more complicated than a
    standard data symbol, since they have a size and can be structures.

    'value' can be a single integer or (more likely) a list/tuple.
    """
    integer_names = [
        "uint32",
        "uint24",
        "uint16",
        "uint8",
        "int",
        "unsigned",
        "unsigned int"
    ]

    def __init__(
            self,
            name,
            address,
            size,
            value=None,
            var_type=None,
            debuginfo=None,
            members=None,
            parent=None
    ):
        DataSym.__init__(self, name, address, value)
        self.size = size
        self.type = var_type  # type ID; meaningless to users

        # list of members (Variable objects).
        # These can be struct/union members, or array elements
        if members is None:
            self.members = []
        else:
            self.members = members
        self.parent = parent  # The Variable which owns this one (if any).
        # if non-zero, variable is an array of this length
        # Note: some types define an array of length 0...
        self.array_len = 0
        # e.g. struct foo, or uint16*
        self.type_name = ''
        # name of this struct/union member, without the parent part.
        self.base_name = ''

        # Thes will be set if the variable is part of a bitfield.
        self.size_bits = None  # Size in bits (self.size will always be '1').
        # Mask that must be ANDed with self.value to yield the correct bitfield
        self.val_mask = None
        # Right-shift that must be applied to self.value to yield the correct
        # bitfield
        self.val_rshift = None

        self.indent = ""  # Used when printing a variable out

        # Debuginfo is used to cast the sturcture fileds for better display.
        self.debuginfo = debuginfo

    def __repr__(self, *args, **kwargs):
        """
        @brief The standard representation will be the same as the standard to
        string to make the interactive interpreter more user-frendly.
        @param[in] self Pointer to the current object
        """
        return self.__str__()

    def __str__(self, debug=False):
        """
        @brief The standard to string function.
        @param[in] self Pointer to the current object
        """
        if debug:
            return self._debug_var_to_str()

        return self.var_to_str()

    def _debug_var_to_str(self):
        """
        @brief The standard to string function used in debug mode. The debug
            mode will display more information about the variable. These
            additional information is more bitfield and address related.
        @param[in] self Pointer to the current object
        """
        mystr = DataSym.__str__(self)
        mystr = self.indent + mystr.replace('\n', '\n' + self.indent)
        # By default, assume we want to poke into every member.
        dont_recurse = False

        # Construct typestr
        typestr = self.type_name
        if self.array_len > 0:
            typestr += '[' + str(self.array_len) + ']'
            if self.array_len > 10:
                dont_recurse = True

        if self.size_bits:
            mystr += ('Size (bitfield): ' + str(self.size_bits) + ' bits \n')
        else:
            mystr += ('Size: ' + str(self.size) + '\n')
        mystr += (self.indent + 'Type: ' + typestr + '\n')

        if self.parent:
            mystr += (self.indent + 'Parent: ' + self.parent.name + '\n')
        # Members
        if self.members and dont_recurse:
            mystr += (self.indent + 'Members: TOO MANY TO LIST\n')

        if self.members and not dont_recurse:
            mystr += (self.indent + 'Members: \n')
            for member in self.members:
                member.indent = self.indent + '  '
                mystr += str(member)
                mystr += member.indent + '----\n'
                member.indent = ""  # Reset the indent once we're done with it

        return mystr


    def _get_enum_name(self):
        """
        @brief Function returns the enum name based on the value.
        @param[in] self Pointer to the current object
        """
        # check if the debug info is set to get the enum names from
        # there.
        ret_string = ""
        if self.debuginfo is not None:
            enum_value_name = ""
            error_msg = ""
            try:
                # get the enum type name which is after the "enum " word
                # 1- is used as an indexer to avoid index error which
                # in this case is favourable
                enum_type_name = self.type_name.split("enum ")[-1]
                enum_value_name = self.debuginfo.get_enum(
                    enum_type_name,
                    self.value
                )
            except InvalidDebuginfoEnum:  # The enum type is missing
                # This can happen if an enum type is typdefed. Something like:
                # typedef enum_type new_enum_type; try to dereference
                try:
                    # get the enum type
                    enum_type_id = self.debuginfo.get_type_info(
                        enum_type_name
                    )[1]
                    # get the referenced type from the enum type.
                    enum_type = self.debuginfo.types[enum_type_id]
                    enum_type = self.debuginfo.types[enum_type.ref_type_id]
                    enum_type_name = enum_type.name
                    # Finally, get the enum value name.
                    enum_value_name = self.debuginfo.get_enum(
                        enum_type.name,
                        self.value
                    )
                except (InvalidDebuginfoType, InvalidDebuginfoEnum):
                    error_msg += (
                        "(enum type \"" + enum_type_name +
                        "\" not found for \"" + self.base_name + "\" member)"
                    )
                except KeyError:  # the enum is missing the values
                    error_msg += (
                        "(enum \"" + enum_type_name +
                        "\" has no value " + cu.hex(self.value) + ")"
                    )
            except KeyError:  # the enum is missing the values
                error_msg += (
                    "(enum \"" + enum_type_name +
                    "\" has no value " + cu.hex(self.value) + ")"
                )

            if enum_value_name == "":
                enum_value_name = cu.hex(self.value) + " " + error_msg
            else:
                if len(enum_value_name) > 1:
                    # There are multiple matches for the values. Display all
                    # of them one after the other.
                    temp_name = ""
                    for value_name in enum_value_name:
                        if temp_name == "":
                            temp_name += "( "
                        elif value_name == enum_value_name[-1]:
                            temp_name += " and "
                        else:
                            temp_name += ", "
                        temp_name += value_name
                    temp_name += " have value " + cu.hex(self.value)
                    temp_name += " in " + enum_type_name + ")"
                    enum_value_name = cu.hex(self.value) + " " + temp_name
                else:
                    # get_enum panics if there are no matches so we are
                    # sure that enum_value_name has at lest one element.
                    temp_name = enum_value_name[0]
                    enum_value_name = temp_name + " " + cu.hex(self.value)
            # concatenate the return string.
            ret_string += self.base_name + ": " + enum_value_name + "\n"
        elif self.base_name != "":
            # Just display the base name and value.
            ret_string += self.base_name + ": " + cu.hex(self.value) + "\n"
        else:
            # Display the value and name.
            ret_string += self.name + ": " + cu.hex(self.value) + "\n"
        return ret_string

    def var_to_str(self, depth=0):
        """
        @brief function used to convert a structure to a base_name: value \\n string
        @param[in] self Pointer to the current object
        @param[in] depth = 0
        """
        depth_str = "  " * depth
        fv_str = ""

        if depth == 0:
            fv_str += "0x%08x " % self.address

        if self.members:
            if self.base_name == "":
                # Probably it was a pointer to something
                fv_str += (depth_str + self.name + ":\n")
            else:
                fv_str += (depth_str + self.base_name + ":\n")
            for member in self.members:
                fv_str += member.var_to_str(depth + 1)
        else:
            part_of_array = False
            if self.parent and \
                    isinstance(self.parent.array_len, int):
                # this member is an element of an array.
                part_of_array = True
                fv_str += (
                    depth_str + "[" + str(self.parent.members.index(self)) +
                    "]"
                )
                # no need to add additional depth string.
                depth_str = ""

            if self.type_name in self.integer_names:
                # display integers in hex
                if not part_of_array:
                    fv_str += (depth_str + self.base_name)
                fv_str += (": " + cu.hex(self.value) + "\n")
            elif "bool" in self.type_name:
                # buleans are displayed as true (1) or false (0).
                fv_str += (depth_str + self.base_name + ": ")
                if self.value != 0:
                    fv_str += ("True\n")
                else:
                    fv_str += ("False\n")
            elif "enum " in self.type_name:
                fv_str += depth_str + self._get_enum_name()
            else:
                # This is probably a pointer to something.
                if not part_of_array:
                    if self.base_name != "":
                        fv_str += (depth_str + self.base_name)
                    else:
                        fv_str += (depth_str + self.name)
                fv_str += (": " + cu.hex(self.value) + "\n")
        return fv_str

    def set_debuginfo(self, debuginfo):
        """
        @brief Sets the debug information for the variable. The debuginfo set
        will be used to better display the variable.
        @param[in] self Pointer to the current object
        @param[in] debuginfo Debug information which will be used to get type
            and enum information.
        """
        self.debuginfo = debuginfo

    def __getitem__(self, key):
        """
        @brief Overload the [] operator so that the variable can be used as an
        array. Only valid when the variable actually is an array.
        @param[in] self Pointer to the current object
        @param[in] key
        """
        if self.array_len > 0:
            item = self.members[key]
            # Politeness: if .value is an array of length 1, turn it into a
            # simple int.
            if item.value and \
                    not isinstance(item.value, cu.INTEGERS) and \
                    len(item.value) == 1:
                item.value = item.value[0]
            return self.members[key]

        return None

    def get_member(self, name):
        """
        @brief Get a variable member by name. Only valid if the variable is a struct
        or union.
        Note: could have overridden the dot operator here, but wary of a clash
        with actual Variable members.
        @param[in] self Pointer to the current object
        @param[in] name
        """
        if not self.members:
            # No members to return!
            return None

        if self.array_len is not None and self.array_len > 0:
            # This is an array; you can't access members by name
            return None

        for member in self.members:
            if member.base_name == name:
                return member
        return None


class SourceInfo(object):
    """
    @brief A bunch of information about a particular address in PM.
    """

    def __init__(self, address, module_name, src_file, line_number):
        self.address = address
        self.module_name = module_name
        self.src_file = src_file
        self.line_number = line_number
        self.nearest_label = None  # a CodeLabel.

        # Note that nearest_label should be filled in on-demand, e.g. by calls
        # to DebugInfo.get_nearest_label(). It's too slow to calculate it
        # up-front for every PM RAM/ROM address.

    def __str__(self):
        if self.nearest_label is None:
            nearest_label_str = "Uncalculated"
        else:
            nearest_label_str = str(self.nearest_label)

        # Using str(x) here since it copes with the value being None
        return (
            'Code address: ' + cu.hex(self.address) + '\n' + 'Module name: ' +
            str(self.module_name) + '\n' + 'Found in: ' + self.src_file +
            ', line ' + str(self.line_number) + '\n' + 'Nearest label is: ' +
            nearest_label_str
        )


class CodeLabel(object):
    """
    @brief Information about a code label
    """

    def __init__(self, name, address):
        """
        @brief Initiaise the object.
        @param[in] self Pointer to the current object
        @param[in] name Code label name.
        @param[in] address Code label address.
        """
        self.name = name
        self.address = address

    def __str__(self):
        """
        @brief Standard to string function.
        @param[in] self Pointer to the current object
        """
        return self.name + ', address ' + cu.hex(self.address) + '\n'
