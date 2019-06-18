############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module to analyse the stack.
"""
import copy
from collections import OrderedDict
import traceback

from ACAT.Core import CoreTypes as ct
from ACAT.Core import CoreUtils as cu
from . import Analysis
from ..Core import Arch

VARIABLE_DEPENDENCIES = {'strict': ("$stack.buffer", "$stack.p1_buffer")}

# Clearly needs to be modified if we ever change the contents of an interrupt
# stack frame. Lowest address first.
INT_FRAME_DESCRIPTORS_HYDRA = [
    'FP',
    'r0',
    'r1',
    'r2',
    'rflags',
    'ARITHMETIC_MODE',
    'MM_RINTLINK',
    'r3',
    'r4',
    'r5',
    'r6',
    'r7',
    'r8',
    'r9',
    'r10',
    'rLink',
    'I0',
    'I1',
    'I2',
    'I3',
    'I4',
    'I5',
    'I6',
    'I7',
    'M0',
    'M1',
    'M2',
    'M3',
    'L0',
    'L1',
    'L4',
    'L5',
    'rMAC2',
    'rMAC1',
    'rMAC0',
    'DoLoopStart',
    'DoLoopEnd',
    'DivResult',
    'DivRemainder',
    'rMACB2',
    'rMACB1',
    'rMACB0',
    'B0',
    'B1',
    'B4',
    'B5',
    'INT_SAVE_INFO'
]

class StackReadFailure(Exception):
    """
    Exception signals stack read failure.
    """
    pass


class StackInfo(Analysis.Analysis):
    """
    @brief This class encapsulates an analysis for a stack backtrace.
    """

    # If you're trying to understand some of the code here it might help to
    # look at http://wiki/AudioCPU/Stack.

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        self.interrupt_frames = []
        if Arch.chip_arch == "Hydra":
            self.int_frame_descriptors = INT_FRAME_DESCRIPTORS_HYDRA
        else:
            # Other architectures not yet supported
            raise Exception("Chip architecture not yet supported")
        # Use Stack properties read from the build.
        self.use_internal_values = False
        # Give some default values for internal stack properties.
        self.start_of_stack = 0
        self.end_of_stack = 0
        self.preset_frame_pointer = 0
        self.preset_stack_pointer = 0

    def _safe_run_all(self):
        """
        @brief Runs all the available analyses on the stack catching exceptions
            if present.
        @param[in] self Pointer to the current object
        """
        try:
            self.analyse_stack_usage()
        except BaseException:
            self.formatter.output(traceback.format_exc())

        try:
            self.analyse_stack_trace()
        except BaseException:
            self.formatter.output(traceback.format_exc())

        try:
            self.display_stack_content()
        except BaseException:
            self.formatter.output(traceback.format_exc())

    def _get_regular_stack_info(self):
        """
        @brief In case of a stack overflow the stack is changed to the exception
            stack. This function can recover the regular stack properties.
        @param[in] self Pointer to the current object
        @param[ou] The regular stack start address and stack end address
        """
        if self.chipdata.processor == 0:
            stack_var = self.debuginfo.get_var_strict("$stack.buffer")
        else:
            stack_var = self.debuginfo.get_var_strict("$stack.p1_buffer")

        start_address = stack_var.address
        end_address = stack_var.address + stack_var.size - Arch.addr_per_word
        return start_address, end_address

    def run_all(self):
        """
        @brief Perform all useful analysis and spew the output to the formatter.
        It outputs how much of the stack is used, if SP and FP are valid and
        the stack trace.
        @param[in] self Pointer to the current object
        """
        self.formatter.section_start('Stack')
        # Check that we haven't overflowed
        if self.stack_overflow_occurred():
            try:
                self._analyse_overflow_stack()
            except BaseException:
                self.formatter.output(traceback.format_exc())
            self.formatter.section_start('Exception Stack')
            self._safe_run_all()
            self.formatter.section_end()
            # set the stack properties to the normal test.
            start_address, end_address = self._get_regular_stack_info()
            self.change_stack_properties(
                start_address, end_address,
                end_address - Arch.addr_per_word,
                end_address
            )

            self.formatter.section_start('Normal Stack')
            self._safe_run_all()
            self.formatter.section_end()
            # use the stack registers again.
            self.use_internal_values = False
            self.cache = {}
        else:
            self._safe_run_all()
        self.formatter.section_end()

    def stack_overflow_occurred(self):
        """
        @brief Have we experienced a stack overflow? (If so, we will have switched
        to using the exception stack.)
        Returns True/False.
        @param[in] self Pointer to the current object
        """
        # Could work this out in a few ways; easiest is to check
        # STACK_OVERFLOW_PC, which should be set if the hardware is doing its
        # job.
        overflow_pc = self._get_stack_overflow_pc()
        return overflow_pc != 0

    def _get_all_stack_data(self):
        """
        @brief Returns all the stack related data.
        @param[in] self Pointer to the current object
        """
        if self.stack_overflow_occurred():
            self._display_alert(
                "Stack overflow has occurred!"
            )

        start_of_stack = self._get_stack_start()
        end_of_stack = self._get_stack_end()
        stack_pointer = self._get_stack_pointer()
        frame_pointer = self._get_frame_pointer()

        # Repeat the same sanity check we do in analyse_spfp; can't think of a
        # good way to modularise this since we don't want this method to do
        # analysis.
        if stack_pointer >= start_of_stack and stack_pointer <= end_of_stack and \
            frame_pointer >= start_of_stack and frame_pointer <= end_of_stack and \
            start_of_stack < end_of_stack:
            pass
        else:
            raise StackReadFailure(
                "Can't get stack! Probably the subsystem wasn't booted.\n" +
                "start_of_stack = 0x%08x, end_of_stack = 0x%08x\n" % (
                    start_of_stack, end_of_stack
                ) +
                "stack_pointer = 0x%08x, frame_pointer = 0x%08x" % (
                    stack_pointer, frame_pointer
                )
            )

        # Ok, if we get here we have no excuse not to at least give it a try.
        # Use _get_stack to save some time on reading the stack once again.
        sp_idx = (stack_pointer - start_of_stack) // Arch.addr_per_word
        the_stack = self._get_stack()[:sp_idx]

        return the_stack, start_of_stack, end_of_stack, stack_pointer, frame_pointer

    def _get_guessed_fp_locations(self, the_stack):
        """
        @brief Returns all the locations in the stack which can be used as FP.
        @param[in] self Pointer to the current object
        """
        # Search for potential frame pointer on the stack.
        location = 0
        # will contain all the indexes of the potential FPs
        potential_stack_addresses = []
        for val in the_stack:
            if self.is_stack_address(val):
                potential_stack_addresses.append(location)
            location = location + 1
        # Because we traverse the stack in the reverse order we need to reverse
        # the list of potential FP locations too.
        potential_stack_addresses.reverse()
        return potential_stack_addresses

    @Analysis.cache_decorator
    def get_stack_frames(self):
        """
        @brief Returns a list of tuples, each containing a stack frame. The oldest
        frame is at the start, and frames are listed with lowest-
        address first - that is, the same way DM normally appears.
        Might raise an AnalysisError exception.
        @param[in] self Pointer to the current object
        """
        # read all the stack data.
        the_stack, start_of_stack, _, _, frame_pointer = \
            self._get_all_stack_data()
        # search for potential frame pointers in case we have a stack corruption.
        potential_fp_locations = self._get_guessed_fp_locations(the_stack)


        all_stackframes = []

        # The first stackframe is between SP and FP.
        # index into the stack array.
        fp_idx = (frame_pointer - start_of_stack) // Arch.addr_per_word
        # Works because the_stack stops at SP. Add the first stack frame.
        all_stackframes.insert(0, the_stack[fp_idx:])  # Insert at start of list.


        # Now work backwards through all the other stack frames. This should end when
        # fp_idx is exactly equal to 0.
        safe_guard = 0
        max_loop = 100
        while (fp_idx > 0) and (safe_guard < max_loop):
            # get the next frame pointer form the stack.
            start_of_next_frame = (
                the_stack[fp_idx] - start_of_stack
            ) // Arch.addr_per_word
            # check if the frame pointer is valid.
            valid_fp = True
            if start_of_next_frame < 0 or start_of_next_frame > fp_idx:
                self._display_alert(
                    "Stack traversal failed! Stack corrupted at address "
                    "0x%08x. The value of the location is 0x%08x which "
                    "cannot be used as a frame pointer(FP)." % (
                        fp_idx * Arch.addr_per_word + start_of_stack,
                        the_stack[fp_idx]
                    )
                )
                valid_fp = False
            elif fp_idx - start_of_next_frame > 100:
                self._display_alert(
                    "Stack traversal failed! Stack frame from "
                    "0x%08x to 0x%08x is too long (%d words)!." % (
                        fp_idx * Arch.addr_per_word + start_of_stack,
                        start_of_next_frame * Arch.addr_per_word + start_of_stack,
                        fp_idx - start_of_next_frame
                    )
                )
                valid_fp = False

            if not valid_fp:
                # search for other potential return addresses
                # in the rest of the stack.
                potential_fp_found = False
                for next_fp_idx in potential_fp_locations:
                    if next_fp_idx >= 0 and next_fp_idx < fp_idx:
                        start_of_next_frame = next_fp_idx
                        potential_fp_found = True
                        break
                # there is no reason to continue we run out of options.
                if not potential_fp_found:
                    break
            this_frame = the_stack[start_of_next_frame:fp_idx]
            # Insert at start of list.
            all_stackframes.insert(0, this_frame)
            fp_idx = start_of_next_frame
            # increment the safe guard to avoid infinite loops.
            safe_guard = safe_guard + 1

        if safe_guard == max_loop:
            self.formatter.alert("Exited because of safe_guard")

        # In case of a memory corruption the "while" loop can exit without
        # consuming all the stack. Add the remaining to the stackframes.
        if fp_idx != 0:
            this_frame = the_stack[0:fp_idx]
            all_stackframes.insert(0, this_frame)

        return all_stackframes

    def _is_interrupt_frame(self, frame_name, frame_length):
        """
        @brief Checks if a given frame is a potential interrupt frame.
        @param[in] self Pointer to the current object
        @param[in] frame_name Name of the frame
        @param[in] frame_length Length of the frame.
        """
        # Frames with "$M.interrupt.handler" name are highly likely interrupt
        # frames
        return (
            frame_name == "$M.interrupt.handler" and
            len(self.int_frame_descriptors) <= frame_length
        )

    def _get_return_addresses(self, stack_frame, frame_name, frame_address):
        """
        @brief Converts a stack frame to a dictionary mapping the addresses
            to values and other additional information. This function also
            returns the guessed return address and next stack frame name which
            is based on the guessed return address.
        @param[in] self Pointer to the current object
        @param[in] stack_frame Array with the stack frame values
        @param[in] frame_address Address from which the frame starts.
        """
        # Mark the next frame as unknown until something interesting is found
        next_frame_name = "Unknown function"
        guessed_return_address = None
        # No need to deep copy as the tuple to list conversion already does
        # that.
        stack_frame = list(stack_frame)
        stack_frame.reverse()
        len_stack_frame = len(stack_frame)
        # create a dictionary to save every information about the stack.
        stack_frame_info = OrderedDict()

        for index, value in enumerate(stack_frame):
            address = frame_address - index * Arch.addr_per_word

            description = ""
            if self.is_saved_rlink(value):
                try:
                    label = self.debuginfo.get_source_info(value)
                    label_name = label.module_name
                    description += "%s %s line: %d" % (
                        label.module_name,
                        # get the file name by removing the path.
                        label.src_file.split("\\")[-1].split("/")[-1],
                        label.line_number
                    )
                except ct.BundleMissingError:
                    label_name = (
                        "No source information for 0x%08X. " % (value) +
                        "Bundle is missing."
                    )
                    description = label_name
                if self._is_interrupt_frame(frame_name, len_stack_frame):
                    # we are inside an interrupt frame. Only accept
                    # return addresses from RLINK. Note: because we are
                    # going in the reverse order -(index + 1) is used.
                    if self.int_frame_descriptors[-(index + 1)] == "rLink":
                        # the next frame name will be ..
                        next_frame_name = label_name
                        guessed_return_address = address

                elif index < len_stack_frame - 1 and not self.is_stack_address(value):
                    # the next frame name will be ..
                    next_frame_name = label_name
                    guessed_return_address = address

            stack_frame_info[address] = {
                "value": value,
                "description": description
            }

        return next_frame_name, guessed_return_address, stack_frame_info

    def _frame_to_string(self, frame, frame_name, frame_address):
        """
        @brief Converts a stack frame to a formatted string displaying any
            potential return addresses and the guessed return address.
        @param[in] self Pointer to the current object
        @param[in] frame Array with the stack frame values
        @param[in] frame_name The name of the frame.
        @param[in] frame_address Address from which the frame starts.
        """
        output_text = ""
        output_text += "  Stack of: %s \n" % (frame_name)

        next_frame_name, guessed_return_address, stack_frame = \
            self._get_return_addresses(frame, frame_name, frame_address)

        # Signal if the stack was corrupted
        last_address = list(stack_frame.keys())[-1]
        if not self.is_stack_address(stack_frame[last_address]["value"]):
            stack_frame[last_address]["description"] = (
                "  - Stack corrupted here!" +
                stack_frame[last_address]["description"]
            )

        frame_length = len(frame)
        for index, address in enumerate(stack_frame):
            # print "address",address
            output_text += (
                "    0x%08x: 0x%08x" %
                (address, stack_frame[address]["value"])
            )

            if self._is_interrupt_frame(frame_name, frame_length):
                # We are in a interrupt frame. Get the name of the saved
                # register.
                len_int_frame = len(self.int_frame_descriptors)
                int_frame_index = len_int_frame - 1 - index
                if int_frame_index >= 0:
                    register_name = self.int_frame_descriptors[int_frame_index]
                else:
                    register_name = "too long interrupt frame"
                output_text += " - %16s" % (register_name)

            # Now display the description of the address
            if stack_frame[address]["description"] != "":
                if address == guessed_return_address:
                    output_text += (
                        " - " +
                        stack_frame[address]["description"]
                    )

                else:
                    output_text += (
                        " (potential return address - " +
                        stack_frame[address]["description"] + ")"
                    )
            output_text += "\n"

        return next_frame_name, output_text

    def display_stack_content(self):
        """
        @brief Displays the stack in a programmer friendly format.
        @param[in] self Pointer to the current object
        """
        # make a copy of the stack frames to avoid modifying it.
        stack_frames = copy.deepcopy(self.get_stack_frames())
        stack_frames.reverse()

        # this variable will hold the output text.
        output_text = ""

        program_counter = self._get_program_counter()
        rlink = self._get_rlink()
        # the first frame name will be based on the program counter.
        try:
            current_frame_name = self.debuginfo.get_source_info(program_counter).module_name
        except ct.BundleMissingError:
            current_frame_name = (
                "No source information for "
                "PC 0x%08X. Bundle is missing." % (program_counter)
            )
        try:
            rlink_info = self.debuginfo.get_source_info(rlink).module_name
        except ct.BundleMissingError:
            rlink_info = (
                "No source information for "
                "RLINK 0x%08X. Bundle is missing." % (rlink)
            )



        simple_stack_backtrace = []
        # Don't forget PC and RLINK - add these at the start.
        simple_stack_backtrace.append(
            "PC - " + current_frame_name
        )
        simple_stack_backtrace.append(
            "RLINK - " + rlink_info + "\n"
        )

        frame_end_address = self._get_stack_pointer() - Arch.addr_per_word
        for frame in stack_frames:
            simple_stack_backtrace.append(current_frame_name)
            next_frame_name, frame_str = \
                self._frame_to_string(frame, current_frame_name, frame_end_address)

            output_text += frame_str
            current_frame_name = next_frame_name
            frame_end_address = frame_end_address - len(frame) * Arch.addr_per_word

            output_text += "\n"

        self.formatter.section_start('Simple stack backtrace')
        self.formatter.output(
            'Note that this is a GUESS. Assembler code can cause confusion.'
        )
        self.formatter.output_list(simple_stack_backtrace)
        self.formatter.section_end()
        # Print out the stack content
        self.formatter.section_start('Stack Content')
        self.formatter.output(output_text)
        self.formatter.section_end()


    def is_stack_address(self, address):
        """
        @brief Takes an address and tries to work out whether or not it points to
        a location on the stack.
        Returns True or False.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        start_of_stack = self._get_stack_start()
        end_of_stack = self._get_stack_end()

        return address >= start_of_stack and address <= end_of_stack

    @staticmethod
    def _maxim_is_call_instruction(instr):
        """
        @brief Function checks if a maxim instruction is a call.
        @param[in] self Pointer to the current object
        @param[in] instr Maxim encoded instruction.
        @param[out] True if the instruction is a call.
        """
        # First 7 bits must be 1110 000 - ie opcode=call, type=a/b
        return instr & 0xfe000000 == 0xe0000000

    @staticmethod
    def _minim_is_call_instruction(instr, is_prefixed):
        """
        @brief Function checks if a minim instruction is a call.
        @param[in] self Pointer to the current object
        @param[in] instr Minim encoded instruction.
        @param[in] is_prefixed Is the given instruction prefixed.
        @param[out] True if the instruction is a call.
        """
        if is_prefixed:
            # Prefixed Type B Call, Prefixed Type A Call in Insert32
            return ((instr &
                     0xF0E0) == 0xE020) or ((instr & 0xEFF0) == 0xCE10)
        # Unprefixed Type B Call, Unprefixed Type A Call
        return ((instr &
                 0xFE00) == 0x4E00) or ((instr & 0xFFF8) == 0x4CD0)

    @staticmethod
    def _minim_is_prefix_instruction(instr, is_prefixed):
        """
        @brief Function checks if a minim instruction is prefixed.
        @param[in] self Pointer to the current object
        @param[in] instr Minim encoded instruction.
        @param[in] is_prefixed Instruction prefixed.
        @param[out] True if the instruction is prefixed.
        """
        if is_prefixed:
            # 32-bit prefix in an Insert32
            return ((instr &
                     0xf000) == 0xf000) or ((instr & 0xfff0) == 0xcfd0)
        return (instr & 0xf000) == 0xf000

    def _maxim_address_is_call(self, address):
        """
        @brief Check if the maxim pm encoded address is a call.
        @param[in] self Pointer to the current object
        @param[in] address PM address with maxim encoding
        @param[out] True if the address holds a call.
        """
        # Maxim.
        # First, check that the address supplied is actually a code
        # address.
        try:
            _ = self.debuginfo.get_instruction(address)
        except ct.InvalidPmAddress:
            return False
        except ct.BundleMissingError:
            # The address is in a missing bundle which is very bad because
            # maxim encoding is not used for downloadable capabilities.
            self.formatter.alert(
                "Bundle missing _maxim_address_is_call returned False"
            )
            return False

        # Now look for the previous Maxim instruction.
        # KAL_ARCH_3 DSPs have word-addressed PM (i.e. previous
        # instruction is at address-1), but others are octet-addressed.
        if Arch.kal_arch == 3:
            prev_address = address - 1
        else:
            # Sanity-check that the address supplied is divisible
            # by 4 (otherwise it's clearly not the address of an
            # instruction).
            if address % 4 != 0:
                return False
            prev_address = address - 4

        # Now look for the previous Maxim instruction.
        try:
            instr_n1 = self.debuginfo.get_instruction(prev_address)
        except ct.InvalidPmAddress:
            return False
        except ct.BundleMissingError:
            # The address is in a missing bundle which is very bad because
            # maxim encoding is not used for downloadable capabilities.
            self.formatter.alert(
                "Bundle missing _maxim_address_is_call returned False"
            )
            return False

        return self._maxim_is_call_instruction(instr_n1)

    def _read_16bit_pm(self, address):
        """
        @brief Helper function to read minim encoded instruction from PM ram.
        @param[in] self Pointer to the current object
        @param[in] address PM address with minim encoding
        """
        # get_data_pm always returns a word stating form
        # address -  address % Arch.addr_per_word.
        value = self.chipdata.get_data_pm(address)
        # Pm ram is little-endian
        if address % Arch.addr_per_word != 0:
            value = value >> 16
        # mask the value to 16bit
        return value & 0xFFFF

    def _minim_address_is_call(self, address):
        """
        @brief Check if the minim pm encoded address is a call.
        @param[in] self Pointer to the current object
        @param[in] address PM address with minim encoding
        @param[out] True if the address holds a call.
        """
        # Minim.
        # This is really quite tricky.
        # First, sanity-check that the address supplied is odd. (A saved
        # minim address will have the LSbit set). Then clear said bit,
        # so we can look up the actual instruction referred-to.
        if address & 1:
            address = address - 1
        else:
            return False

        # Now, we need to work out whether the instruction at (n-1) has a
        # prefix; need to know that before we can determine whether it is
        # a call instruction. The only way to do that is to look at (n-2).
        # Except THAT might have a prefix too, so we also need to look at
        # (n-3) as well. Thankfully Kalimba only permits a maximum of two
        # prefixes so we can stop there.
        # Any chip that supports Minim will also have octet-addressed PM,
        # so at least we don't have to make this even more complicated.
        try:
            # Previous Minim instruction; suspected 'call'.
            instr_n1 = self.debuginfo.get_instruction(address - 2)
            # n-2
            instr_n2 = self.debuginfo.get_instruction(address - 4)
            # n-3
            instr_n3 = self.debuginfo.get_instruction(address - 6)

        except ct.BundleMissingError:
            # double check if the address is in PM ram in case this function
            # is reused somewhere other than is_saved_rlink.
            if Arch.get_pm_region(address, False) != "PMRAM":
                return False
            # We cannot read the instruction value from the bundle,
            # but the address is in PM ram so we can read it from
            # the chip.
            # Previous Minim instruction; suspected 'call'.
            instr_n1 = self._read_16bit_pm(address - 2)
            # n-2
            instr_n2 = self._read_16bit_pm(address - 4)
            # n-3
            instr_n3 = self._read_16bit_pm(address - 6)
        except ct.InvalidPmAddress:
            # If we hit any errors in the above, it's probably because
            # (address - n) isn't actually a valid PM address.
            return False

        # Hopefully clear from the comment above.
        double_prefix = self._minim_is_prefix_instruction(instr_n3, False)
        if self._minim_is_prefix_instruction(instr_n2, double_prefix):
            # Test-with-prefix
            return self._minim_is_call_instruction(instr_n1, True)

        # No prefix
        return self._minim_is_call_instruction(instr_n1, False)

    def is_saved_rlink(self, address):
        """
        @brief Takes an address from the stack and tries to work out
        whether or not it is a saved value of rLink.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        if Arch.get_pm_region(address, False) is None:
            # Clearly not in PM
            return False

        # We need to look up the instruction *before* the supplied address
        # and work out whether it is a call statement.
        try:
            is_maxim = self.debuginfo.is_maxim_pm_encoding(address)
        except ct.BundleMissingError:
            # The address is in a missing bundle. All downloadable capabilities
            # are in PM RAM and they will always use minim encoding.
            # is_maxim_pm_encoding may not know this if a bundle is missing.
            if Arch.get_pm_region(address, False) == "PMRAM":
                is_maxim = False
            else:
                return False
        except ct.UnknownPmEncoding:
            # Address wasn't even in static PM.
            return False

        if is_maxim:
            # The address has maxim encoding
            return self._maxim_address_is_call(address)

        # The address has minim encoding
        return self._minim_address_is_call(address)

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def analyse_stack_usage(self):
        """
        @brief How much stack have we used? Are SP and FP valid?
        @param[in] self Pointer to the current object
        """
        # Re-use our 'where is the stack' code.
        the_stack = self._get_stack()
        start_of_stack = self._get_stack_start()
        end_of_stack = self._get_stack_end()

        stack_pointer = self._get_stack_pointer()
        frame_pointer = self._get_frame_pointer()

        if stack_pointer >= start_of_stack and stack_pointer <= end_of_stack and \
            frame_pointer >= start_of_stack and frame_pointer <= end_of_stack and \
            start_of_stack < end_of_stack:
            pass
        else:
            self._display_alert(
                "Incorrect stack settings. Probably the subsystem wasn't booted."
            )
        self.formatter.output(
            'Stack runs from 0x%x - 0x%x' % (start_of_stack, end_of_stack)
        )
        self.formatter.output('SP = 0x%x, FP = 0x%x' % (stack_pointer, frame_pointer))

        if stack_pointer >= start_of_stack and stack_pointer <= end_of_stack:
            pass
        else:
            self.formatter.alert('SP does not point to a stack address!')

        if frame_pointer >= start_of_stack and frame_pointer <= end_of_stack:
            pass
        else:
            self.formatter.alert('FP does not point to a stack address!')

        # We normally expect SP to be greater than FP
        if stack_pointer < frame_pointer:
            self.formatter.alert('SP is not greater than FP!')

        # Work out how much stack we have left.
        self.formatter.output(
            '%d words of stack free' %
            ((end_of_stack - stack_pointer) // Arch.addr_per_word)
        )

        # Work out our peak stack usage. Do this simply by counting the number
        # of words at the end of the stack which have never been set (where
        # 'never been set' means 'are zero'; not 100% accurate if you're
        # going to go around putting a bunch of zeroes on the stack).
        min_stack_space = len(the_stack)
        for index in range(len(the_stack) - 1, -1, -1):
            if the_stack[index] != 0:
                # maximum index is len(the_stack) - 1
                min_stack_space = len(the_stack) - 1 - index
                break

        self.formatter.output(
            '%d words minimum stack space' %
            (min_stack_space)
        )

    def analyse_stack_trace(self):
        """
        @brief Go through all stack frames and try to work out a coherent stack trace.
        'detailed' is a flag telling us whether to splurge all the gritty
        details about each location. Most of the time this is overkill.
        @param[in] self Pointer to the current object
        @param[in] detailed = False
        """
        stack_frames = copy.deepcopy(self.get_stack_frames())
        # Reverse the list of stack frames, because we want to display/analyse
        # the most-recent stuff first.
        stack_frames.reverse()

        self.formatter.section_start('Stack frames')
        self.formatter.output_list(stack_frames)
        self.formatter.section_end()

    def change_stack_properties(
            self, start_of_stack, end_of_stack, frame_pointer, stack_pointer
        ):
        """
        @brief Displays an alert once per cache clear (per ACAT command).
        @param[in] self Pointer to the current object
        @param[in] start_of_stack Address where the stack starts.
        @param[in] end_of_stack Address where the stack starts.
        @param[in] frame_pointer Address where the frame pointer points to.
        @param[in] stack_pointer Address where the stack pointer points to.
        """
        if (start_of_stack >=
                end_of_stack) or (frame_pointer >= stack_pointer):
            self.formatter.error("Invalid configuration!")
            return

        # Cached data by Analysis.cache_decorator must be cleared.
        self.cache = {}

        self.start_of_stack = start_of_stack
        self.end_of_stack = end_of_stack
        self.preset_frame_pointer = frame_pointer
        self.preset_stack_pointer = stack_pointer
        # From now onwards use the internally stored stack properties.
        self.use_internal_values = True

    #######################################################################
    # Private methods - don't call these externally.
    #######################################################################

    def _display_alert(self, alert):
        """
        @brief Displays an alert once per cache clear (per ACAT command).
        @param[in] self Pointer to the current object
        """
        # The cache is used to make sure the alert is only displayed once per
        # call.
        if not alert + "alert_displayed" in self.cache:
            self.cache[alert + "alert_displayed"] = True
            # The cache will be cleared automatically
            self.formatter.alert(alert)

    @Analysis.cache_decorator
    def _get_stack_pointer(self):
        """
        @brief Reads the stack pointer (SP) register of the chip.
        @param[in] self Pointer to the current object
        """
        if self.use_internal_values:
            return self.preset_stack_pointer

        stack_pointer = self.chipdata.get_reg_strict('REGFILE_SP').value
        if not self.is_stack_address(stack_pointer):
            sp_temp = self._get_stack_end()
            self._display_alert(
                "SP 0x%08x is not a valid stack address! "
                "0x%08x will be used instead." % (stack_pointer, sp_temp)
            )
            stack_pointer = sp_temp

        return stack_pointer

    @Analysis.cache_decorator
    def _get_frame_pointer(self):
        """
        @brief Reads the frame pointer (FP) register of the chip.
        @param[in] self Pointer to the current object
        """
        if self.use_internal_values:
            return self.preset_frame_pointer

        frame_pointer = self.chipdata.get_reg_strict('REGFILE_FP').value
        if not self.is_stack_address(frame_pointer):
            # FP should be the at least one word less than the SP
            fp_temp = self._get_stack_pointer() - Arch.addr_per_word
            self._display_alert(
                "FP 0x%08x is not a valid stack address! "
                "0x%08x will be used instead." % (frame_pointer, fp_temp)
            )
            frame_pointer = fp_temp
        return frame_pointer

    @Analysis.cache_decorator
    def _get_program_counter(self):
        """
        @brief Reads the program counter (PC) register of the chip.
        @param[in] self Pointer to the current object
        """
        program_counter = self.chipdata.get_reg_strict('REGFILE_PC').value
        if Arch.get_pm_region(program_counter, False) is None:
            self._display_alert(
                "PC 0x%08x is not a valid program memory address! "
                "0x%08x will be used instead." % (program_counter, 0)
            )
            program_counter = 0
        return program_counter

    @Analysis.cache_decorator
    def _get_rlink(self):
        """
        @brief Reads the rlink register of the chip.
        @param[in] self Pointer to the current object
        """
        rlink = self.chipdata.get_reg_strict('REGFILE_RLINK').value
        if Arch.get_pm_region(rlink, False) is None:
            self._display_alert(
                "rlink 0x%08x is not a valid program memory address! "
                "0x%08x will be used instead." % (rlink, 0)
            )
            rlink = 0
        return rlink

    @Analysis.cache_decorator
    def _get_stack_start(self):
        """
        @brief Reads the start address of the stack.
        @param[in] self Pointer to the current object
        """
        if self.use_internal_values:
            return self.start_of_stack

        start_of_stack = self.chipdata.get_reg_strict(
            '$STACK_START_ADDR'
        ).value
        return start_of_stack

    @Analysis.cache_decorator
    def _get_stack_end(self):
        """
        @brief Reads the end address of the stack.
        @param[in] self Pointer to the current object
        """
        if self.use_internal_values:
            return self.end_of_stack

        end_of_stack = self.chipdata.get_reg_strict('$STACK_END_ADDR').value
        return end_of_stack

    @Analysis.cache_decorator
    def _get_stack(self):
        """
        @brief Get hold of the entire stack.
        Returns a Variable encapsulating the entire stack buffer (use
        get_stack_frames if you want it split into frames).
        Might raise an AnalysisError exception, if something is suspicious.
        @param[in] self Pointer to the current object
        """
        if self.chipdata.is_volatile():
            self._display_alert(
                "Warning: connected to live chip -- stack display may be incorrect."
            )
        start_of_stack = self._get_stack_start()
        end_of_stack = self._get_stack_end()
        # It is enough to supply the address of the stack
        # (which is the value of the STACK_START_ADDR
        # register) to get the stack.buffer variable.
        # This is useful because for P1 it has a different
        # name (stack.p1_buffer).
        the_stack = self.chipdata.get_data(
            start_of_stack, (end_of_stack - start_of_stack)
        )

        # Load the necessary register to ACAT's cache to make sure that
        # the their read is very close to the actual stack read.
        self._get_stack_pointer()
        self._get_frame_pointer()
        self._get_program_counter()
        self._get_rlink()
        # self._get_stack_start() is already called above
        self._get_stack_end()
        self._get_stack_overflow_pc()

        return the_stack

    @Analysis.cache_decorator
    def _get_stack_overflow_pc(self):
        """
        @brief Reads the stack overflow pc.
        @param[in] self Pointer to the current object
        """
        overflow_pc = self.chipdata.get_reg_strict('$STACK_OVERFLOW_PC').value
        if Arch.get_pm_region(overflow_pc, False) is None:
            self._display_alert(
                "Overflow PC 0x%08x is not a valid program memory address! "
                "0x%08x will be used instead." % (overflow_pc, 0)
            )
            overflow_pc = 0
        return overflow_pc

    def _analyse_overflow_stack(self):
        """
        @brief Method to display a stack overflow.
        @param[in] self Pointer to the current object
        """
        if self.chipdata.is_volatile():
            self._display_alert(
                "Warning: connected to live chip -- The stack can be corrupted."
            )
        self._display_alert('Stack overflow has occurred!')

        # Try to work out when we overflowed. STACK_OVERFLOW_PC
        # should be set.
        overflow_pc = self._get_stack_overflow_pc()

        try:
            # a SourceInfo object
            culprit = self.debuginfo.get_source_info(overflow_pc)
            self.formatter.output('\nOverflow occurred in:')
            self.formatter.output(cu.add_indentation(str(culprit), 4))
        except ct.InvalidPmAddress:
            # overflow_pc.value was not a code address. Give up.
            self.formatter.output(
                'STACK_OVERFLOW_PC is 0x%08x - no help there! ' % overflow_pc
            )
        except ct.BundleMissingError:
            self.formatter.output(
                "No source information for "
                "overflow PC 0x%08X. Bundle is missing." % (overflow_pc)
            )
