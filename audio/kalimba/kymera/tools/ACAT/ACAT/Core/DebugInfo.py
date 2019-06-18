############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
The interface to the debug information is defined in this module.
"""
import abc
from ACAT.Core import CoreTypes as ct
from . import Arch

##################################################
# This is an abstract base class, but I am avoiding use of Python's ABC syntax.
class DebugInfoInterface(object):
    """
    @brief This class defines an interface for extracting debug information from
    the build output.
    """
    __metaclass__ = abc.ABCMeta

    def __init__(self):
        pass

    @abc.abstractmethod
    def get_cap_data_type(self, cap_name, elf_id=None):
        """
        @brief Returns the data type name for a given capability.
            Note: The data type is used to hold all the information for a
            capability.
        @param[in] self Pointer to the current object
        @param[in] cap_name Capability name.
        @param[out] The name of the extra data type for the given capability.
            returns None if no information found.
        """

    @abc.abstractmethod
    def get_constant_strict(self, name, elf_id=None):
        """
        @brief Return value is a ConstSym object (which may be None).
        If 'name' is not the exact name of a known constant, a KeyError
        exception will be raised.
        @param[in] self Pointer to the current object
        @param[in] name Name of the constant
        @param[in] elf_id Used by DebugInfoAdapter to select the debug
            information.
        """

    @abc.abstractmethod
    def get_var_strict(self, identifier, elf_id=None):
        """
        @brief Search list of variables for the supplied identifier (name or
        address). If 'identifier' is not the exact name or start address of a
        known variable, a KeyError exception will be raised.
        ('identifier' is interpreted as an address if it is an integer, and a
        name otherwise)
        @param[in] self Pointer to the current object
        @param[in] identifier
        @param[in] elf_id Used by DebugInfoAdapter to select the debug
            information.
        """

    @abc.abstractmethod
    def get_dm_const(self, address, length=0, elf_id=None):
        """
        @brief Get the contents of DM const(NVMEM); returns an integer.
        If the address is out of range, a KeyError exception will be raised.
        If the address is valid but the length is not (i.e. address+length
        is not a valid address) an OutOfRange exception will be raised.
        @param[in] self Pointer to the current object
        @param[in] address
        @param[in] length Default 0
        """

    @abc.abstractmethod
    def get_source_info(self, address):
        """
        @brief Get information about a code address (integer) or module name
        (string) in PM.
        Return value is a SourceInfo object.
        @param[in] self Pointer to the current object
        @param[in] address
        """

    @abc.abstractmethod
    def get_nearest_label(self, address):
        """
        @brief Finds the nearest code label to the supplied address.
        Return value is a CodeLabel object.
        @param[in] self Pointer to the current object
        @param[in] address
        """

    def get_label(self, address):
        """
        @brief Finds the code label at the exact supplied address.
        Return value is a CodeLabel object, None if no label found
        at the address.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        label = self.get_nearest_label(address)
        if label is not None:
            if Arch.kal_arch == 4:
                # for kal_arch4 clear minim bit
                address -= address & 0x1
            if address != label.address:
                # expect label at exact address
                label = None
        if label is None:
            raise ct.DebugInfoNoLabel()
        return label

    @abc.abstractmethod
    def get_instruction(self, address):
        """
        @brief Return the contents of Program Memory at the supplied address.
        The width of the return value depends on whether the instruction is
        encoded as Minim (16-bit) or Maxim (32-bit).
        Will throw a KeyError exception if the address is not in PM RAM/ROM,
        or the address is not the start of an instruction.
        @param[in] self Pointer to the current object
        @param[in] address
        """

    @abc.abstractmethod
    def get_enum(self, enum_name, member=None, elf_id=None):
        """
        @brief If supplied just with a name, returns a dictionary mapping member
        name->value.
        If also supplied with a member name/value, returns a tuple containing
        any matching values/names.

        e.g.
        get_enum('ACCMD_CON_TYPE') - returns a dictionary of all enum members
        get_enum('ACCMD_CON_TYPE', 'ACCMD_CON_TEST') - returns value of
                                ACCMD_CON_TEST member (1 in this case)
        get_enum('ACCMD_CON_TYPE', 1) - returns the name(s) of any entries
                                with value 1 ('ACCMD_CON_TEST' in this case)

        If either enum_name or member is not found, a KeyError exception will
        be raised.
        @param[in] self Pointer to the current object
        @param[in] enum_name
        @param[in] member = None
        @param[in] elf_id Used by DebugInfoAdapter to select the debug
            information.
        """

    @abc.abstractmethod
    def get_type_info(self, type_name_or_id, elf_id=None):
        """
        @brief Takes a type name (e.g. 'ENDPOINT' or 'audio_buf_handle_struc')
        or a valid typeId, and looks up information in the type database.
        Returns a tuple containing:
        * The (fully-qualified) name of the type.
        * The typeid (redundant if 'type' is already a typeid)
        * The pointed-to typeid (if the type is a pointer)
        * The array length (if the type or pointed-to type is an array).
        * The typeid which describes any members of this type (if it is a
          structure or union).
        * Type size in addressable units (if the type or pointed-to type is a
          structure or union)

        If an error occurs, an empty tuple is returned.
        @param[in] self Pointer to the current object
        @param[in] type_name_or_id
        @param[in] elf_id Used by DebugInfoAdapter to select the debug
            information.
        """

    @abc.abstractmethod
    def read_const_string(self, address, elf_id=None):
        """
        @brief Takes the address of a (filename) string in const, returns a
        string.
        @param[in] self Pointer to the current object
        @param[in] address
        @param[in] elf_id Used by DebugInfoAdapter to select the debug
            information.
        """

    @abc.abstractmethod
    def inspect_var(self, var, elf_id=None):
        """
        @brief Inspects a variable. Inspecting a variable will build the
            variable members.
        @param[in] self Pointer to the current object
        @param[in] var Variable to inspect
        @param[in] elf_id=None
        """

    @abc.abstractmethod
    def is_maxim_pm_encoding(self, address):
        """
        @brief Look up the contents of Program Memory at the supplied address,
            and work out whether it is encoded as Maxim or Minim.
            Returns:
                True - if the encoding is Maxim.
                False - if encoding is Minim.
                raises InvalidPmAddress - if address is not in PM
                raises UnknownPmEncoding - if address has unknown encoding.
        @param[in] self Pointer to the current object
        @param[in] address Address to check.
        @param[out] Returns True if code encoding is maxim.
        """

    @abc.abstractmethod
    def is_pm_private(self, address):
        """
        @brief Checks if the pm address is private or not
        @param[in] self Pointer to the current object
        @param[in] address pm address
        @param[out] True if address private, false otherwise.
        """

    @abc.abstractmethod
    def get_mmap_lst(self, elf_id):
        """
        Returns the mmap_lst for the boundle.
        @param[in] elf_id The bundle elf id.
        """
