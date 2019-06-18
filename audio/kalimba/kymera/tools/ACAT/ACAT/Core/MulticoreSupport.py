############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module responsible to enable dual core support.
"""
import os
import copy

from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from ACAT.Core.DownloadedCapability import MappedTable
from ACAT.Core import Arch
from ACAT.Core.KerDebugInfo import KerDebugInfo
from ACAT.Core.DebugInfoAdapter import DebugInfoAdapter as DebugInformation


def load_boundle(bundle_path):
    """
    @brief Loads a bundle (also known as KDCs) and puts it to a dictionary
    based on the elf id. NOTE; a bundle is kept in an elf file.
    @param[in] bundle_path Path to the boundle
    """
    # Dict representing a bundle
    bundle_dictionary = {}

    # Create separate Debug info for each bundle
    bundle_debuginfo = KerDebugInfo()
    bundle_debuginfo.read_debuginfo(bundle_path)
    bundle_dictionary[bundle_debuginfo.elf_id] = bundle_debuginfo

    return bundle_dictionary


def get_build_output_path(chipdata):
    """
    @brief Reads the build ID from the chip and tries to search for the
    released build based on the ID. Returns the path to the release build.
    @param[in] chipdata Access to the chip.
    """
    # If the build_output_path was not supplied, try to work it out from the
    # build ID. This obviously doesn't work for unreleased builds.
    build_id = chipdata.get_firmware_id()
    build_output_path = None

    if build_id == 0xFFFF:
        # Unreleased build!
        raise ct.Usage(
            "ERROR: Path to build output not supplied, "
            "and build is unreleased!")
    else:
        try:
            from ACAT.Core.BuildFinder import BuildFinder
            build_finder = BuildFinder()
            build_output_path = os.path.join(
                build_finder.get_build_dir(build_id), 'debugbin'
            )
        except ImportError:
            raise ct.Usage("ERROR: Path to build output not supplied.")

    return build_output_path


class Functions(object):
    """
    @brief Class which encapsulates the functions used in Interactive mode.
    """

    def __init__(self, chipdata, debuginfo, formatter):
        self.chipdata = chipdata
        self.debuginfo = debuginfo
        self.formatter = formatter

    def get(self, identifier, elf_id=None, datalen=None):
        """
        @brief A 'smart' lookup funtion.
        'identifier' can be an address, or the name of a register or variable
        (note constant names are not handled). If it's a name, we attempt to
        find the closest match in our lists of registers and variables.
        If the identifier is ambiguous, an AmbiguousSymbol exception may be
        thrown.
        Like get_var, it's also possible to pass in a data length to request
        a slice of data (this only makes sense for things in DM).
        Returns a DataSym (which may be a Variable) or a SourceInfo (if a
        code address was supplied).

        Obviously there are things that could go wrong here, especially if
        the user passes in a random number and we try to guess what it points
        to. That's why we only provide this function in Interactive mode!
        @param[in] self Pointer to the current object
        @param[in] identifier Could be name or address
        @param[in] elf_id The bundle elf id if the variable is in a
                            downloadable capability.
        @param[in] datalen If the identifier is an address the data length is
                            specified by this input.
        """

        pm_addr = None
        reg = None
        var = None
        apology = ""
        hit_flag = False
        # For Crescendo, data can only be fetched as words. Since it is
        # octet-addressed, the addresses must be divisible with the number of
        # addresses per word (32 bit words - 4 octets, therefore addresses must
        # be divisble with 4).
        if isinstance(identifier, cu.INTEGERS):
            identifier = cu.get_correct_addr(identifier, Arch.addr_per_word)

        # Same as above. The lengths are measured in addressable units.
        if datalen is not None:
            datalen = cu.convert_byte_len_word(datalen, Arch.addr_per_word)

        # Look to see whether it's an address in PM.
        # Since the PM range can be huge (and may overlap with DM), perform a
        # sanity check to avoid false positives.
        if isinstance(identifier, cu.INTEGERS) and \
            (Arch.get_pm_region(identifier, False) == "PMROM" or \
            Arch.get_pm_region(identifier, False) == "PMRAM") and \
            datalen is None:
            # Is it the address of a valid instruction?
            try:
                _ = self.debuginfo.get_instruction(identifier)
            except KeyError:
                pass
            else:
                pm_addr = self.debuginfo.get_source_info(identifier)

        # Look to see whether it's the name of a module in PM. This needs to
        # include all the fluff at the start, (i.e. '$M.foo.bar' rather than
        # just 'foo.bar' or 'bar') so as to avoid clashes with names in DM
        # space (which are more likely to be what's wanted).
        if isinstance(identifier, cu.INTEGERS):
            try:
                pm_addr = self.debuginfo.get_source_info(identifier)
                hit_flag = True
            except KeyError:
                pass
            except ct.AmbiguousSymbol as amb_pm:
                apology += str(amb_pm) + "\n"

        # Look to see whether it's a register.
        try:
            reg = self.get_reg(identifier)
            hit_flag = True
        except ct.AmbiguousSymbol as amb_reg:
            apology += str(amb_reg) + "\n"

        # Also look to see whether it's a variable name/address.
        try:
            var = self.get_var(identifier, elf_id, datalen)
            hit_flag = True
        except ct.AmbiguousSymbol as amb_var:
            apology += str(amb_var) + "\n"

        # We got at least one hit
        if hit_flag:
            if not pm_addr and not reg and not var:
                # We didn't find anything at all
                return None
            elif pm_addr and not reg and not var:
                return pm_addr
            elif reg and not var and not pm_addr:
                return reg
            elif var and not reg and not pm_addr:
                return var
            else:
                # We got unique, valid hits for one or more of the above.
                apology = "Multiple potential matches found for identifier '" + \
                    str(identifier) + "': \n"

        # If we get here then we couldn't resolve ambiguous symbols in one or more cases.
        # Note any genuine match of the other types as well.
        if pm_addr:
            apology += " code address in module " + pm_addr.module_name + "\n"
        if var:
            apology += " variable " + var.name + "\n"
        if reg:
            apology += " register " + reg.name + "\n"
        raise ct.AmbiguousSymbol(apology)

    def get_reg(self, identifier):
        """
        @brief Like Analysis.get_reg_strict(), except it's not strict (!)
        'identifier' can be a register name, or address. If it's a name, we
        attempt to find the closest match in our list of registers.
        If more than one match is found, an AmbiguousSymbol exception is
        raised.

        Returns a DataSym.
        @param[in] self Pointer to the current object
        @param[in] identifier
        """

        reg = None  # Will be a ConstSym.

        # If the user supplied an address, and it smells like a register,
        # attempt to look it up.
        if isinstance(identifier, cu.INTEGERS):
            if Arch.get_dm_region(identifier) == "MMR":
                # Look for constants that have a value of the supplied address.
                # Inherently risky, since any random constant could have a value
                # that looks like a register address.
                # Since we only do this to set the name, it should be ok.
                possible_regs = [
                    item[0] for item in list(self.debuginfo.constants.items())
                    if item[1] == identifier
                ]
                if possible_regs:
                    reg_name = " or ".join(possible_regs)
                    reg = ct.ConstSym(reg_name, identifier)
        else:
            # Look up the supplied name in our list of constants.
            # If the name is found, reg.value is actually going to be the address
            # of the register.

            # get_constant might throw an AmbiguousSymbol exception here; in this
            # case we want to catch it, and weed out any matches that aren't
            # register names.
            try:
                if 'regfile' in identifier:
                    return self.chipdata.get_reg_strict(identifier)

                return self.chipdata.get_reg_strict(
                    'regfile_' + identifier
                )
            except KeyError:
                pass
            except BaseException:
                # This shoud be on UnknownRegister but is too hard to import
                pass

            try:
                reg = self.debuginfo.get_constant(identifier)
            except ct.AmbiguousSymbol as ambs:
                # We helpfully store the ambiguous matches in the exception
                # args
                ambiguous_matches = ambs.args[1]

                actual_ambiguous_matches = []
                for match in ambiguous_matches:
                    amconst = self.debuginfo.get_constant_strict(
                        match["name"], match["elf_id"]
                    )
                    if Arch.get_dm_region(amconst.value, False) == "MMR":
                        actual_ambiguous_matches.append(match)

                if not actual_ambiguous_matches:
                    # We actually ended up finding no real registers
                    reg = None
                else:
                    # If all the matches are aliases for each other, we can
                    # return that value. if they're different,
                    # admit our mistake.
                    val = self.debuginfo.get_constant_strict(
                        actual_ambiguous_matches[0]["name"],
                        actual_ambiguous_matches[0]["elf_id"]
                    )
                    # The first is always the same with the first.
                    success = True
                    # Skip the first which is used to check against.
                    for match in actual_ambiguous_matches[1:]:
                        try:
                            if val != self.debuginfo.get_constant_strict(
                                    match["name"], match["elf_id"]
                                ):
                                success = False
                                break
                        # Todo remevoe this if B-242063 is corrected.
                        except BaseException:
                            success = False
                            break

                    if success:
                        # We actually got only one register match - work with
                        # it.
                        reg = self.debuginfo.get_constant_strict(
                            actual_ambiguous_matches[0]["name"]
                        )
                    else:
                        apology = "Multiple potential matches found " + \
                            "for register name '" + identifier + "': "
                        raise ct.AmbiguousSymbol(
                            apology, actual_ambiguous_matches)
            try:
                if reg and (Arch.get_dm_region(reg.value) != "MMR"):
                    # Reg.value wasn't the address of a memory-mapped register; it
                    # was probably a random symbolic constant. Oh well.
                    return None
            except Arch.NotDmRegion:
                if reg.value == 0xfffffff:
                    # For Crescendo, it has been noticed that the register are
                    # being treated as constants with the value 0xfffffff.
                    # Furthermore, must strip the C and asm specific symbols
                    # for get_reg_strict().
                    try:
                        if '$_' in reg.name:
                            reg_name = reg.name[2:]
                            return self.chipdata.get_reg_strict(reg_name)
                        elif '$' in reg.name:
                            reg_name = reg.name[1:]
                            return self.chipdata.get_reg_strict(reg_name)
                    except BaseException:
                        return self.chipdata.get_reg_strict(reg.name)

        if reg is None:
            return None

        # If we get here, we found something.
        # There's a small chance we've got an unrelated constant, if its
        # value looks sufficiently like the address of a memory-mapped
        # register (e.g. $codec.stream_decode.FAST_AVERAGE_SHIFT_CONST).
        # Can't do much about that.

        # Look up the register contents.
        try:
            regcontents = self.chipdata.get_data(reg.value)
            fullreg = ct.DataSym(reg.name, reg.value, regcontents)
        except KeyError:
            # Reg.value wasn't the address of a register, for some reason.
            fullreg = None

        return fullreg

    def get_var(self, identifier, elf_id=None, datalen=None):
        """
        @brief Like Analysis.get_var_strict(), except it's not strict (!)
        'identifier' can be a variable name, or address. If it's a name, we
        attempt to find the closest match in our list of variables.
        If more than one match is found, an AmbiguousSymbol exception is
        raised.

        In this case the user can also provide a data length; if it is set,
        we return a slice of data, 'datalen' addressable units long starting at
        the address specified by 'identifier'.
        Returns a Variable
        @param[in] self Pointer to the current object
        @param[in] identifier Could be name or address
        @param[in] elf_id The bundle elf id if the variable is in a
                            downloadable capability.
        @param[in] datalen If the identifier is an address the data length is
                            specified by this input.
        """
        # For Crescendo, data can only be fetched as words. Since it is
        # octet-addressed, the addresses must be divisible with the number of
        # addresses per word (32 bit words - 4 octets, therefore addresses must
        # be divisble with 4).
        if isinstance(identifier, cu.INTEGERS):
            identifier = cu.get_correct_addr(identifier, Arch.addr_per_word)

        # Same as above. The lengths are measured in addressable units.
        if datalen is not None:
            datalen = cu.convert_byte_len_word(datalen, Arch.addr_per_word)
        # The following is necessary since we can't rely on variable
        # sizes. If a (say) register address was passed in here we will likely
        # match a variable entry for $flash.data24.__Limit.
        if isinstance(identifier, cu.INTEGERS) and \
                Arch.get_dm_region(identifier) == "MMR":
            return None

        # First, look up the variable in the debug information.
        # Even if the user supplied an address rather than a name, it's nice
        # if we can tell them which variable it might be part of.

        # Might throw an AmbiguousSymbol exception here; can't get that with
        # an address but can with a variable name.
        var = None
        try:
            var = self.debuginfo.get_var(identifier, elf_id)
        except ct.AmbiguousSymbol as amb:
            # Filter out matches of struct/array members, where their parent is
            # also in the list of matches.
            matches = amb.args[1]
            quarantine_list = []
            for match in matches:
                try:
                    mvar = self.debuginfo.get_var_strict(
                        match["name"], match["elf_id"]
                    )
                    if mvar.parent is not None and mvar.parent.name in matches:
                        # This is a struct/array member
                        quarantine_list.append(match)
                    else:
                        possible_parent = mvar
                except ValueError:
                    # Out of memory can be seen for asm memory reagions. Ignore
                    # them.
                    quarantine_list.append(match)

            # If the number of things in the quarantine list is EXACTLY ONE MORE
            # than the number of things in the matches list, then we probably
            # have found a single variable and all its members.
            if len(matches) == len(quarantine_list) + 1:
                var = possible_parent
            else:
                # Give up
                raise ct.AmbiguousSymbol(amb.args[0], amb.args[1])

        if var is None:
            return None

        # Don't necessarily want to modify the actual variable entry below*,
        # so maybe create a copy here.
        # * Why? Well var is just a reference to the original variable in the
        # debuginfo class - we ought not to change it frivolously, since it
        # could break some other analysis.
        # In this case, we don't want to permanently munge the name
        # just because we're doing a slice this time.
        ret_var = var
        if datalen:
            if isinstance(identifier, cu.INTEGERS):
                if var.address == identifier and var.size <= datalen:
                    ret_var = copy.deepcopy(var)
                    ret_var.name = "User-defined slice, part of: " + \
                        var.name + " ???"
                    # We want to get a slice of data, not just the variable
                    # entry.
                    ret_var.size = datalen
                    # If the identifier is a variable name, don't include any
                    # members we might have already inspected.
                    ret_var.members = None
                else:
                    ret_var = ct.Variable("???", identifier, datalen)
        else:
            # Mitigation: we can't rely on 'var' actually containing the
            # supplied address, due to the lack of size information (see
            # KerDebugInfo.py). So work around it here.
            if isinstance(identifier, cu.INTEGERS) and \
                    identifier >= (var.address + Arch.addr_per_word * var.size):
                # Just return the value at the given address.
                ret_var = ct.Variable(var.name + " ???", identifier, 1)

        # Now get the data value(s) from chipdata. Look in DM first, only
        # try const if we run out of options.
        try:
            ret_var.value = self.chipdata.get_data(
                ret_var.address, ret_var.size
            )
        except ct.InvalidDmLength as oor:
            # Address was valid, but size was not.
            # oor.args[1] contains the last valid address in the supplied
            # range.
            valid_size = (oor.max_length - ret_var.address) + 1
            ret_var.value = self.chipdata.get_data(ret_var.address, valid_size)
            ret_var.size = valid_size
        except ct.InvalidDmAddress:
            # The address wasn't valid. Could be that this variable is
            # actually in dm const.
            try:
                ret_var.value = self.debuginfo.get_dm_const(
                    ret_var.address, ret_var.size
                )
            except ct.InvalidDmConstLength as oor:
                # Address was valid, but size was not.
                valid_size = oor.max_length - ret_var.address
                ret_var.value = self.debuginfo.get_dm_const(
                    ret_var.address, valid_size
                )
            except ct.InvalidDmConstAddress:
                # Ok we really are stuck. Return variable with a value of None.
                ret_var.value = self.debuginfo.get_kymera_debuginfo().debug_strings[ret_var.address]
                return ret_var

        # Need a way to work out whether we've already inspected this
        # variable, so we can avoid doing it more than once.
        # An inspection *should* result in a non-empty type_name string.
        # Also, don't inspect the slices. It would be bad.
        ret_var.members = []
        var_elf_id = self.debuginfo.table.get_elf_id_from_address(
            ret_var.address
        )
        if not var_elf_id:
            var_elf_id = self.debuginfo.get_kymera_debuginfo().elf_id

        self.debuginfo.inspect_var(ret_var, var_elf_id)
        return ret_var


#########################################


class Processor(Functions):
    """
    @brief Class which creates objects with specific information for all the cores.
    """

    def __init__(self, coredump_path, build_output_path, processor, formatter):
        """
        @brief Initialise the specific information for a core: chipdata,
        debuginfo and the functions used in Interactive mode that use the
        specific information.
        The formatter is included because of the differences between the
        Automatic and Interactive modes.
        @param[in] self Pointer to the current object
        @param[in] coredump_path
        @param[in] build_output_path
        @param[in] processor
        @param[in] formatter
        """

        self.processor = processor
        self.available_analyses = {}
        if cu.global_options.live:
            if cu.global_options.kalcmd_object is not None:
                from ACAT.Core.LiveKalcmd import LiveKalcmd
                self.chipdata = LiveKalcmd(
                    cu.global_options.kalcmd_object, self.processor
                )
            else:
                from ACAT.Core.LiveSpi import LiveSpi
                if self.processor == 0:
                    self.chipdata = LiveSpi(
                        cu.global_options.kal,
                        cu.global_options.spi_trans,
                        self.processor,
                        wait_for_proc_to_start=cu.global_options.
                        wait_for_proc_to_start
                    )
                else:
                    self.chipdata = LiveSpi(
                        cu.global_options.kal2,
                        cu.global_options.spi_trans,
                        self.processor,
                        wait_for_proc_to_start=False
                    )
        else:
            from ACAT.Core.Coredump import Coredump
            self.chipdata = Coredump(coredump_path, processor)

        if build_output_path == "":
            # Try to get the build path automatically. If this fails it will
            # throw an exception and we'll bail.
            build_output_path = get_build_output_path(self.chipdata)

            # search for the elf file name
            import glob
            elf_files = glob.glob(os.path.join(build_output_path, '*.elf'))
            # Filter out the "_external.elf" files generated by some _release
            # builds -- we want the corresponding internal one with full
            # symbols (which we assume must exist).
            elf_files = [
                elf_file for elf_file in elf_files
                if not elf_file.endswith("_external.elf")
            ]
            if len(elf_files) > 1:
                raise ct.Usage(
                    "ERROR: Multiple elf files in the build output, "
                    "don't know which to use.")
            # remove the .elf extension
            build_output_path = elf_files[0].replace(".elf", "")
            cu.global_options.build_output_path_p0 = build_output_path
            cu.global_options.build_output_path_p1 = build_output_path

        if processor == 0:
            if cu.global_options.kymera_p0 is None:
                self.debuginfo = DebugInformation(cu.global_options.ker)
                self.debuginfo.read_kymera_debuginfo(build_output_path)
                cu.global_options.kymera_p0 = self.debuginfo

                # If the two build are the same set the main build for the
                # other processor
                if cu.global_options.build_output_path_p0 == \
                        cu.global_options.build_output_path_p1:

                    cu.global_options.kymera_p1 = \
                        cu.global_options.kymera_p0
            else:
                self.debuginfo = cu.global_options.kymera_p0
        else:
            if cu.global_options.kymera_p1 is None:
                self.debuginfo = DebugInformation(cu.global_options.ker)
                self.debuginfo.read_kymera_debuginfo(build_output_path)
                cu.global_options.kymera_p1 = self.debuginfo

                # If the two build are the same set the main build for the
                # other processor
                if cu.global_options.build_output_path_p0 == \
                        cu.global_options.build_output_path_p1:

                    cu.global_options.kymera_p0 = \
                        cu.global_options.kymera_p1
            else:
                self.debuginfo = cu.global_options.kymera_p1

        # load the patches
        if cu.global_options.patch is not None:
            patches = load_boundle(cu.global_options.patch)
            self.debuginfo.update_patches(patches)

        # check if there are any bundles that needs reading
        if cu.global_options.bundle_paths is not None:
            # check if they were already read
            if cu.global_options.bundles is None:
                # if not than read all of them
                bundles_dictionary = {}
                for bundle_path in cu.global_options.bundle_paths:
                    bundles_dictionary.update(load_boundle(bundle_path))
                # and save it in global_options to avoid reading them multiple
                # times.
                cu.global_options.bundles = bundles_dictionary

            # set the bundle dictionary.
            self.debuginfo.update_bundles(cu.global_options.bundles)

        self.debuginfo.set_table(MappedTable(self.chipdata, self.debuginfo))

        # Set the debug info for the chipdata
        self.chipdata.set_debuginfo(self.debuginfo)

        self.formatter = formatter
        Functions.__init__(self, self.chipdata, self.debuginfo, self.formatter)

    def is_booted(self):
        """
        @brief Returns true if the processor was booted, false otherwise. The
        booted state is decided by the Program Counter value.
        @param[in] self Pointer to the current object
        """
        return self.chipdata.is_booted()

    def __getattribute__(self, name):
        """
        @brief Check ONLY for processor 1 if it has been booted before running an analysis.
        @param[in] self Pointer to the current object
        """
        available_analyses = object.__getattribute__(self, "available_analyses")

        if not name in available_analyses:
            # Default behaviour
            return object.__getattribute__(self, name)

        proc_nr = object.__getattribute__(self, "chipdata").processor
        if proc_nr != 1:
            # Default behaviour
            return object.__getattribute__(self, name)
        
        booted = object.__getattribute__(self, "is_booted")()
        if booted:
            # Default behaviour
            return object.__getattribute__(self, name)

        raise ct.AnalysisError("Processor 1 not booted!")

    def get_analysis(self, name):
        """
        @brief Returns the analysis asked by name.
        @param[in] self Pointer to the current object
        @param[in] name Name of the analyses.
        """
        return self.available_analyses[name]
