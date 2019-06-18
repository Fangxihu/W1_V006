############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2016 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
DebugInfoAdapter is used to read Kymera elf and the downloadable bundles
transparently to the user. This module implements the DebugInfoInterface.
"""
import copy

from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from . import KerDebugInfo
from . import DebugInfo as di
from . import Arch


BUNDLE_ERROR_MSG = "Bundle with elf id {0} is missing!\n"\
    "Use -j option or load_bundle(r\"<path>\") in interactive\n"\
    "mode to add bundles to ACAT!"

##############################################################################


class DebugInfoAdapter(di.DebugInfoInterface):
    """
    @brief This class gets debug information from kalelfreader
    """

    def __init__(self, ker):
        """
        @brief Standard initialisation function.

        @note self.debug_infos contains all the available debug information.
        The key None is a special one which keep the patch and Kymera debug
        info. The rest is just Debug info mapped based on the elf id.

        self.debug_infos = {
            None: ["patch", "kymera"],
            "pathc_elf_id": "patch",
            "kymera_elf_id": "kymera",
            "bundle_elf_id_1": "bundle_1"
            "bundle_elf_id_1": "bundle_1"
            ....
        }
        @param[in] self Pointer to the current object
        @param[in] ker Kalimba .elf file reader object
        """
        super(DebugInfoAdapter, self).__init__()
        self.ker = ker
        self.table = None

        patch = None
        kymera = None
        self.debug_infos = {
            None: [patch, kymera]
        }

        # which functions need address mapping before call.
        self.address_mappable_function = [
            "get_var_strict",
            "get_dm_const"

        ]

    ##################################################
    # Debug info interface
    ##################################################

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
        return self._call_debuginfo_elfid(
            elf_id,
            "get_cap_data_type",
            cap_name
        )

    def get_constant_strict(self, name, elf_id=None):
        """
        @brief Return value is a ConstSym object (which may be None).
        If 'name' is not the exact name of a known constant, a KeyError
        exception will be raised.
        @param[in] self Pointer to the current object
        @param[in] name Name of the constant
        @param[in] elf_id Which debug information to use. If None, patch and
            Kymera's debug information is used.
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "get_constant_strict",
            name
        )

    def get_var_strict(self, identifier, elf_id=None):
        """
        @brief Search list of variables for the supplied identifier (name or address).
        If 'identifier' is not the exact name or start address of a known
        variable, a KeyError exception will be raised.
        ('identifier' is interpreted as an address if it is an integer, and a
        name otherwise)
        @param[in] self Pointer to the current object
        @param[in] identifier Name of the variable or pointer to it.
        @param[in] elf_id Which debug information to use. If None, patch and
            Kymera's debug information is used.
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "get_var_strict",
            identifier
        )

    def get_dm_const(self, address, length=0, elf_id=None):
        """
        @brief If the address is out of range, a KeyError exception will be raised.
        If the address is valid but the length is not (i.e. address+length
        is not a valid address) an OutOfRange exception will be raised.
        @param[in] self Pointer to the current object
        @param[in] address Address to read from.
        @param[in] length
        @param[in] elf_id Which debug information to use. If None, patch and
            Kymera's debug information is used.
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "get_dm_const",
            address, length
        )


    def get_source_info(self, address):
        """
        @brief Get information about a code address (integer).
        Return value is a SourceInfo object.
        Could raise a KeyError if the address is not valid.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        try:
            return self._call_debuginfo_pm_addr(address, "get_source_info")
        except ct.DebugInfoNoLabel:
            name = "Unknown function (symbol may be censored)"
            return_sym = ct.SourceInfo(
                address, name, "Unknown file (symbol may be censored)", 0
            )
            return_sym.nearest_label = None
            return return_sym

    def get_nearest_label(self, address):
        """
        @brief Finds the nearest label to the supplied code address.
        Return value is a CodeLabel object.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        return self._call_debuginfo_pm_addr(address, "get_nearest_label")

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
        return self._call_debuginfo_pm_addr(address, "get_instruction")

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
        @param[in] elf_id = None
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "get_enum",
            enum_name, member
        )

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
        * Type size in addressable units (if the type or pointed-to type is a structure
          or union)

        Note: Unfortunately, a small number of types are defined as
        an array, but have a length of 0. That means to determine whether or
        not the type is an array you have to compare array length to None,
        not zero.

        Consider the cases:
         - pointer to an array (we set pointed_to_type, and also array_len
         - array of pointers (we set array_len, but not pointed_to_type)
         - array of pointers to structs (array length is set, members typeid
           defines pointer type, not struct type)
        @param[in] self Pointer to the current object
        @param[in] type_name_or_id
        @param[in] elf_id=None
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "get_type_info",
            type_name_or_id
        )

    def read_const_string(self, address, elf_id=None):
        """
        @brief Takes the address of a (filename) string in const, returns a
            string.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "read_const_string",
            address
        )

    def inspect_var(self, var, elf_id=None):
        """
        @brief Inspects a variable. Inspecting a variable will build the
            variable members.
        @param[in] self Pointer to the current object
        @param[in] var Variable to inspect
        @param[in] elf_id=None
        """
        return self._call_debuginfo_elfid(
            elf_id,
            "inspect_var",
            var
        )

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
        return self._call_debuginfo_pm_addr(address, "is_maxim_pm_encoding")

    def is_pm_private(self, address):
        """
        @brief Checks if the pm address is private or not
        @param[in] self Pointer to the current object
        @param[in] address pm address
        @param[out] True if address private, false otherwise.
        """
        return self._call_debuginfo_pm_addr(address, "is_pm_private")

    def get_mmap_lst(self, elf_id):
        """
        Returns the mmap_lst for the boundle.
        @param[in] elf_id The bundle elf id.
        """
        if elf_id is None:
            raise ct.InvalidDebuginfoCall(
                "get_mmap_lst needs exact elf ID."
                "None can be patch or Kymera debug information"
            )
        try:
            debug_info = self.debug_infos[elf_id]
        except KeyError:
            raise ct.BundleMissingError()
        return debug_info.get_mmap_lst(elf_id)

    ##################################################
    # Adapter Interface
    ##################################################

    def set_table(self, table):
        """
        @brief Sets the table for the adapter
        @param[in] self Pointer to the current object
        @param[in] table
        """
        self.table = table

    def update_patches(self, patches):
        """
        @brief Updates the downloaded bundles for the adapter.
        Note: The update is similar to a dictionary merge.
        @param[in] self Pointer to the current object
        @param[in] bundles A dictionary of bundles
        """
        # A patch can overwrite debug info from the main build. Therefore
        # it must be saved to the special None key.
        count = 0
        for _, patch in patches.items():
            self.debug_infos[None][0] = patch
            count += 1

        if count > 1:
            raise Exception(
                "Only one patchpoint debug information source is supported."
            )

        self.debug_infos.update(patches)

    def update_bundles(self, bundles):
        """
        @brief Updates the downloaded bundles for the adapter.
        Note: The update is similar to a dictionary merge.
        @param[in] self Pointer to the current object
        @param[in] bundles A dictionary of bundles
        """
        for _, debug_info in bundles.items():
            debug_info.set_bundle(True)
        self.debug_infos.update(bundles)

    def read_kymera_debuginfo(self, paths):
        """
        @brief Reads all the bundles (also known as KDCs) and puts it to a
        dictionary based on the elf id. NOTE; a bundle is kept in an elf file.
        @param[in] self Pointer to the current object
        @param[in] bundle_paths list of bundles
        """
        kymera_debug_info = KerDebugInfo.KerDebugInfo()
        try:
            kymera_debug_info.read_debuginfo(paths)
        except TypeError:
            raise Exception("The Pythontools version is too old.")
        # Add to the special key None
        self.debug_infos[None][1] = kymera_debug_info
        # Add to the general dictionary
        self.debug_infos[kymera_debug_info.elf_id] = kymera_debug_info

    def is_elf_loaded(self, elf_id):
        """
        Checks if the elf file is loaded.
        @param[in] elf_id The bundle elf id.
        """
        return elf_id in self.debug_infos

    def _map_data_from_file_to_build(self, var, elf_id):
        '''
        @brief this method maps variable referred to local addresses of
            downloaded capability back to global addresses and values.
        @param[in] var variable.
        @param[in] elf_id elf id to whom the variable structure definition
            belongs to.
        '''
        # Deep copy is used for the value to avoid modifying the original value.
        var = ct.Variable(
            name=var.name,
            address=var.address,
            size=var.size,
            value=copy.deepcopy(var.value),
            var_type=var.type,
            members=var.members,
            parent=var.parent,
            debuginfo=var.debuginfo
        )
        var.address = self.table.convert_addr_to_build(
            var.address, elf_id
        )
        return var

    def _address_in_table(self, address):
        """
        @brief Method to check if and address is in the downloaded boundle
        table.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        if self.table is not None and self.table.is_addr_in_table(address):
            return True

        return False

    ##################################################
    # Adapter Searchable Interface
    ##################################################

    def get_constant(self, name):
        """
        @brief Gets a symbolic constant.
        If 'name' is not the exact name of a known constant, a number of
        variations will be tried in an attempt to find a match.

        This method should only be used in interactive mode; it's risky
        to call it as part of an analysis. Use get_constant_strict instead.

        Return value is a ConstSym object (which may be None).
        An AmbiguousSymbol exception will be raised if the supplied name can't
        be matched to a single constant.
        @param[in] self Pointer to the current object
        @param[in] name
        """

        # todo add search for downloadable capabilities
        matches = self._search_by_name("constants", "search_matches", name)

        if matches == []:
            result = None
        else:
            # We may have found multiple matches for the symbol name.
            # If all the matches are aliases for each other, we can return that value.
            # if they're different, admit our mistake.
            val = self.get_constant_strict(
                matches[0]["name"], matches[0]["elf_id"]
            )
            # The first is always the same with the first.
            success = True
            # Skip the first which is used to check against.
            for match in matches[1:]:
                try:
                    if val != self.get_constant_strict(
                            match["name"], match["elf_id"]
                        ):
                        success = False
                        break
                # Todo remevoe this if B-242063 is corrected.
                except BaseException:
                    success = False
                    break

            if success:
                result = val
            else:
                apology = "Multiple potential matches found for constant '" + name + "': "
                raise ct.AmbiguousSymbol(apology, matches)

        return result

    def get_var(self, identifier, elf_id=None):
        """
        @brief Search list of variables for the supplied identifier (name or address).

        If a supplied name is not the exact name of a known variable, a number
        of variations will be tried in an attempt to find a match.

        If a supplied address is not the start of a known variable, the
        containing variable (if any) will be returned.
        For example, get_var(0x600) and get_var(0x604) will both
        return the variable $stack.buffer, starting at address 0x600.

        ('identifier' is interpreted as an address if it is an integer, and a
        name otherwise)

        This method should only be used in interactive mode; it's risky
        to call it as part of an analysis. Use get_var_strict instead.

        Return value is a Variable object (which may be None).
        An AmbiguousSymbol exception will be raised if the supplied name can't
        be matched to a single variable.
        @param[in] self Pointer to the current object
        @param[in] identifier Could be name or address
        @param[in] elf_id The bundle elf id if the variable is in a
                            downloadable capability.
        """

        if isinstance(identifier, cu.INTEGERS):
            return self._search_var_by_address(identifier)

        # First search for strict matches
        matches = self._search_by_name(
            "var_by_name", "strict_search_matches", identifier, elf_id
        )

        if matches == []:
            # No strict matches: search if there is any similarly named
            # variables
            matches = self._search_by_name(
                "var_by_name", "search_matches",
                identifier, elf_id
            )

        if matches == []:
            result = None
        else:
            # We found one or more matches for the symbol name.
            # If all the matches are aliases for each other, just return that value.
            # if they're different, admit our mistake.
            val = self.get_var_strict(matches[0]["name"], matches[0]["elf_id"])
            # The first is always the same with the first.
            success = True
            # Skip the first which is used to check against.
            for match in matches[1:]:
                try:
                    if val != self.get_var_strict(
                            match["name"], match["elf_id"]
                        ):
                        success = False
                        break
                # Todo remevoe this if B-242063 is corrected.
                except BaseException:
                    success = False
                    break

            if success:
                result = val
            else:
                apology = "Multiple potential matches found for variable '" + identifier + "': "
                raise ct.AmbiguousSymbol(apology, matches)

        return result

    def get_kymera_debuginfo(self):
        """
        @brief Returns Kymera's debug information.

        @param[in] self Pointer to the current object
        @param[ou] Kymera's debug information.
        """
        # Kymera is the last debuginfo in the special list mapped to key None.
        # See __init__ for more.
        return self.debug_infos[None][1]

    def get_patch_debuginfo(self):
        """
        @brief Returns the patch's debug information.

        @param[in] self Pointer to the current object
        @param[ou] patch's debug information or None if patch is not loaded.
        """
        # The patch debug info is the first debuginfo in the special list
        # mapped to key None. See __init__ for more.
        return self.debug_infos[None][0]

    def _search_by_name(self, search_dict_name, function_name, name, elf_id=None):
        """
        @brief Searches for a variable by name. Accepts partial matches.

        @param[in] self Pointer to the current object
        @param[in] name Name to search for.
        @oaram[in] function_name Function name used for searching.
            search_matches and strict_search_matches
        @param[in] elf_id The bundle elf id if the variable is in a
                            downloadable capability.
        @param[ou] matches List of matches.
        """
        ret_matches = []
        if elf_id is None:
            # no elf  id so search everywhere
            for cur_elf_id, debug_info in self.debug_infos.items():
                # elf_id None is used for Kymera and patches, but they can be
                # found based on the id too. So don't double check them.
                if cur_elf_id is not None:
                    function_to_call = debug_info.__getattribute__(function_name)
                    ret_matches += function_to_call(
                        search_dict_name, name
                    )
        else:
            # search in a specific elf
            debug_info = self.debug_infos[elf_id]
            function_to_call = debug_info.__getattribute__(function_name)
            ret_matches += function_to_call(search_dict_name, name)

        return ret_matches

    @staticmethod
    def _check_for_addr(variable_by_address, address):
        # Start off at the address given and work downwards.
        checkaddr = address
        while not (checkaddr in variable_by_address) and checkaddr >= 0:
            checkaddr -= Arch.addr_per_word
        if checkaddr >= 0:
            # We found something
            return variable_by_address[checkaddr]
        return None

    def _search_var_by_address(self, address):
        """
        @brief Searches for a variable by address.

        @param[in] self Pointer to the current object
        @param[in] address Address to search for.
        @param[ou] matches List of matches.
        """
        # If address provided maps to address for downloaded capability, the
        # variable we are searching for is actually stored in downloaded
        # capability
        if self._address_in_table(address):
            elf_id_from_address = self.table.get_elf_id_from_address(address)
            variable_by_address = self.debug_infos[elf_id_from_address].var_by_addr
            return self._check_for_addr(variable_by_address, address)

        # search in Patches and Kymera
        for debug_info in self.debug_infos[None]:
            variable_by_address = debug_info.var_by_addr
            retval = self._check_for_addr(variable_by_address, address)
            if retval:
                return retval

        return None


    def _call_debuginfo_pm_addr(self, address, function_name):
        """
        @brief Searches for the right debuginfo (Kymera or downloadable
            bundles) and call the function specified at the input.

        @param[in] self Pointer to the current object
        @param[in] address Code address.
        @param[ou] function_name Name of the function to call.
        """
        if Arch.get_pm_region(address, False) is None:
            raise ct.InvalidPmAddress("Key " + cu.hex(address) + " is not in PM")

        if self._address_in_table(address):
            # elf_id_from_address should be different than None
            elf_id_from_address = self.table.get_elf_id_from_address(
                address
            )
            if elf_id_from_address not in self.debug_infos:
                # No bundles are loaded to ACAT at all
                raise ct.BundleMissingError(
                    BUNDLE_ERROR_MSG.format(
                        cu.hex(elf_id_from_address)
                    )
                )

            address = self.table.convert_addr_to_download(
                address,
                elf_id_from_address
            )

        else:
            # Use the main Kymera debug info and the patches.
            elf_id_from_address = None

        return self._call_debuginfo_elfid(
            elf_id_from_address, function_name,
            address
        )

    def _call_debuginfo_function(self, elf_id, function_name, *argv, **kwargs):
        """
        @brief Searches for the right debuginfo based on elf id (Kymera or
            downloadable bundles) and call the function specified at the input.

        @param[in] self Pointer to the current object
        @param[in] elf_id The Debug info elf id to call.
        @param[in] function_name Name of the function to call.
        @param[in] argv Function unnamed input parameter.
        @param[in] kwargs Function named input parameter.
        @param[out] Value returned by the function call.
        """
        # convert addresses to the Bundle address if needed
        if function_name in self.address_mappable_function and \
            isinstance(argv[0], cu.INTEGERS):
            address = argv[0]
            if self._address_in_table(address):
                # Get the elf ID from the address.
                elf_id = self.table.get_elf_id_from_address(
                    address
                )
                address = self.table.convert_addr_to_download(
                    address,
                    elf_id
                )
                argv = (address,)

        # Access the debuginfo in the standard way. Do this after the proper
        # elf_id is selected.
        try:
            debug_info = self.debug_infos[elf_id]
        except KeyError:
            raise ct.BundleMissingError()

        # get the function from the debuginfo which will be called.
        function_to_call = debug_info.__getattribute__(function_name)
        # todo: Make all adapted functions to accept elf_id as an input
        # kwargs["elf_id"] = elf_id
        # and call the function.
        return_val = function_to_call(*argv, **kwargs)

        # Remap the addresses from bundle to chip
        if function_name in self.address_mappable_function and \
            debug_info.is_bundle():
            return_val = self._map_data_from_file_to_build(return_val, elf_id)
        return return_val

    def _call_debuginfo_elfid(self, elf_id, function_name, *argv, **kwargs):
        """
        @brief Searches for the right debuginfo based on elf id (Kymera or
            downloadable bundles) and call the function specified at the input.

        @param[in] self Pointer to the current object
        @param[in] elf_id Debug info elf id.
        @param[in] function_name Name of the function to call.
        @param[in] argv Function unnamed input parameter.
        @param[in] kwargs Function named input parameter.
        @param[out] Value returned by the function call.
        """
        if elf_id is None:
            # elf_id == None. Meaning to look for patch and then kymera
            pacth, kymera = self.debug_infos[elf_id]
            if pacth:
                try:
                    return self._call_debuginfo_function(
                        pacth.elf_id,
                        function_name, *argv, **kwargs
                    )
                except ct.DebugInfoError:
                    # this is just the pach so ignore any erros.
                    pass

            # try the same for Kymera. This time do not handle the
            return self._call_debuginfo_function(
                kymera.elf_id,
                function_name, *argv, **kwargs
            )


        return self._call_debuginfo_function(
            elf_id, function_name,
            *argv, **kwargs
        )
