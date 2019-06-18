############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module which implements the heap allocation logic.
"""
import abc
from collections import namedtuple

from ACAT.Core import CoreTypes as ct
from ACAT.Analysis import Analysis
from ACAT.Core import Arch
from ACAT.Core import CoreUtils as cu


FreeBlocks = namedtuple('FreeBlocks', ['total_size', 'info'])

class Heap(Analysis.Analysis):
    """
    @brief This class encapsulates an analysis for heap memory usage.
    """
    heap_names = []
    max_num_heaps = len(heap_names)
    memory_type = "Not set"

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        # Look up the debuginfo once. Don't do it here though; we don't want
        # to throw an exception from the constructor if something goes
        # wrong.
        self.pmalloc_debug_enabled = None


    def display_configuration(self):
        """
        @brief Prints out the heap configuration for both processors.
        @param[in] self Pointer to the current object
        """
        pass

    def display_memory_map(self):
        """
        @brief Displays memory map based on current settings.
        @param[in] self Pointer to the current object
        """
        pass

    def display_heaps_blocks(self):
        """
        @brief Displays all the free and allocated blocks for all the heap
            available in the build.
        @param[in] self Pointer to the current object
        """
        for heap_num in range(self.max_num_heaps):
            # get information about the current heap.
            (available, heap_size, heap_start, _, heap_free_start) = \
                self._get_heap_property(heap_num)

            # Check if the heap is available in the build.
            if available:
                self.formatter.section_start(self.heap_names[heap_num])
                # Analyse the free blocks.
                free_blocks = self._free_blocks(
                    heap_free_start,
                    heap_start,
                    heap_size,
                    memory_type=self.memory_type
                )
                self.formatter.output_list(
                    free_blocks.info
                )
                # Search for allocated blocks using the magic value.
                (heapinfo, debuginfo) = self._alloc_blocks(
                    heap_start, heap_size,
                    memory_type=self.memory_type
                )
                self.formatter.output_list(heapinfo)
                # if debug is enabled print out the extra information.
                if self.pmalloc_debug_enabled:
                    self.formatter.section_start(
                        self.heap_names[heap_num] + ' Debug Info'
                    )
                    self.formatter.output_list(debuginfo)
                    self.formatter.section_end()

                self.formatter.section_end()

    def run_all(self):
        """
        @brief Perform all useful analysis and spew the output to the formatter.
        Displays the heap memory usage and the pmalloc debug info.
        @param[in] self Pointer to the current object
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        self.formatter.section_start(
            'Heap %s Memory Info' % (self.memory_type.upper())
            )

        self.display_configuration()
        self.display_memory_map()
        self.overview()
        self.display_heaps_blocks()

        self.formatter.section_end()

    def _get_heap_config_str(self, processor_number):
        """
        @brief Returns the string representation of the heap configuration .
            In other words is the to string function for processor
            configuration.
        @param[in] self Pointer to the current object
        @param[in] processor_number
        @param[out] processor configuration string.
        """
        return_str = ""
        total_size = 0
        for heap_num in range(self.max_num_heaps):
            (available, heap_size, heap_start, heap_end) = \
                self._get_heap_config(processor_number, heap_num)

            if available:
                return_str += "%-15s : " % self.heap_names[heap_num]
                return_str += "%s at 0x%08x - 0x%08x\n" % (
                    cu.mem_size_to_string(heap_size),
                    heap_start,
                    heap_end
                )

            total_size += heap_size

        return_str += "%-15s : " % "Total size" + cu.mem_size_to_string(total_size)
        return return_str


    def _get_free_space(self):
        """
        @brief This method reads the free blocks for all heaps and returns a
            string which contains a list with the total size of those free
            blocks.
        @param[in] self Pointer to the current object
        """
        # Look up the debug information.
        self._lookup_debuginfo()
        output_str = ""
        total_size = 0
        for heap_num in range(self.max_num_heaps):
            (available, heap_size, heap_start, _, heap_free_start) = \
                self._get_heap_property(heap_num)

            if available:
                free_blocks = self._free_blocks(
                    heap_free_start,
                    heap_start,
                    heap_size,
                    memory_type=self.memory_type
                )
                # display the heap name
                output_str += "%-15s : " % self.heap_names[heap_num]
                free_block_size = free_blocks.total_size
                if self.pmalloc_debug_enabled:
                    # Two words are used for guard.
                    free_block_size -= 2 * Arch.addr_per_word
                output_str += cu.mem_size_to_string(
                    free_block_size
                )
                total_size += free_block_size

                output_str += "\n"
        output_str += "%-15s : " % "Total size" + cu.mem_size_to_string(total_size) + "\n"
        return output_str

    def _get_overview_str(self, detailed=True):
        """
        @brief This method returns an overview string of the available heaps.
            This is a helper method.
        @param[in] self Pointer to the current object
        @param[in] detailed If set to True, a detailed view is provided.
        """
        total_heap, free_heap, min_free_heap = self.ret_get_watermarks()

        output_str = ""
        output_str += (
            "%s heap total size :\n" % (self.memory_type.upper())
        )
        if detailed:
            config_str = self._get_heap_config_str(self.chipdata.processor)
            # add indentation for a nicer view.
            output_str += cu.add_indentation(config_str, 2) + "\n"
        else:
            watermark_str = "%-15s : " % "from config"
            watermark_str += cu.mem_size_to_string(total_heap)
            output_str += cu.add_indentation(watermark_str, 2) + "\n"

        output_str += (
            "%s heap current free:\n" % (self.memory_type.upper())
        )
        if detailed:
            free_space_str = self._get_free_space()
            output_str += cu.add_indentation(free_space_str, 2)

        if free_heap != -1:
            watermark_str = "%-15s : " % "from watermarks"
            watermark_str += cu.mem_size_to_string(free_heap)
        else:
            watermark_str = "Watermarks not available for this heap!"
        output_str += cu.add_indentation(watermark_str, 2) + "\n"

        output_str += (
            "%s heap minimum free:\n" % (self.memory_type.upper())
        )

        if min_free_heap != -1:
            watermark_str = "%-15s : " % "from watermarks"
            watermark_str += cu.mem_size_to_string(min_free_heap)
        else:
            watermark_str = "Watermarks not available for this heap!"
        output_str += cu.add_indentation(watermark_str, 2) + "\n"
        return output_str

    def overview(self):
        """
        @brief This method displays an overview of the available heaps.
        @param[in] self Pointer to the current object
        """
        output_str = self._get_overview_str(detailed=True)

        self.formatter.section_start(
            'Heap %s memory usage' % (self.memory_type.upper())
            )
        # use output_raw to keep the spaces.

        # display memory used by patches for PM Heap
        if self.memory_type.upper() == "PM":
            try:
                patch_analysis = self.interpreter.get_analysis("patches", self.chipdata.processor)
                if patch_analysis.get_patch_level() > 0:
                    patch_address_start = self.debuginfo.get_constant_strict(
                        '$PM_RAM_P0_CODE_START'
                    ).value
                    self.formatter.output_raw(
                        "\nPatch size : " +
                        cu.mem_size_to_string(patch_analysis.get_patch_size(), "o") +
                        " at address: 0x{0:0>8x}".format(patch_address_start) + "\n"
                    )
            except KeyError:
                # Patch analysis is not released to customers.
                pass

        self.formatter.output_raw(output_str)
        self.formatter.section_end()

    def get_watermarks(self):
        """
        @brief Returns the minimum available memory for the total, the free
        and the minimum free memories.
        @param[in] self Pointer to the current object
        """
        output_str = self._get_overview_str(detailed=False)
        # use output_raw to keep the spaces.
        self.formatter.output_raw(output_str)

    def is_address_valid(self, address):
        """
        @brief Returns True if the address belongs to a heap, or False otherwise.
        @param[in] self Pointer to the current object
        @param[in] address Address to verify.
        """
        for heap_number in range(self.max_num_heaps):
            (_, _, start, end, _) = self._get_heap_property(heap_number)
            if (address >= start) and (address <= end):
                return True

        return False

    def _free_blocks(self, address, heap_start, heap_size, memory_type="dm"):
        """
        @brief Checks the free blocks.
        @param[in] self Pointer to the current object
        @param[in] address Address to start with.
        @param[out] Returns a list describing the free memory allocations.
        """
        free_blocks_info = []
        address_history = []
        total_size = 0
        while address != 0:
            # Avoid infinite loop by checking if the node was already checked.
            if address not in address_history:
                address_history.append(address)
            else:
                self.formatter.error(
                    "Repeating nodes with address 0x%x. Probably memory corruption" %
                    address)
                return FreeBlocks(total_size, free_blocks_info)
            if memory_type == "dm":
                freeblock = self.chipdata.cast(address, 'mem_node')
                freeblock_size = freeblock.get_member('length').value
            elif memory_type == "pm":
                freeblock = self.chipdata.cast(address, 'mem_node_pm', False, 'PM')
                freeblock_size = freeblock.get_member(
                    'struct_mem_node'
                ).get_member('length_32').value * Arch.addr_per_word
            else:
                raise ct.FatalAnalysisError(
                    "Memory type %s not recognised." % memory_type
                )

            # verify if the address is valid
            if self.is_address_valid(address):
                # and if the list node belongs to the current analysed heap display info
                if (address >= heap_start) and \
                    (address <= (heap_start + heap_size)):
                    desc_str = (
                        "Free block size : " +
                        cu.mem_size_to_string(freeblock_size, "o") +
                        " at address: 0x{0:0>8x}".format(address)
                    )
                    free_blocks_info.append(desc_str)
                    total_size += freeblock_size
            else:
                raise ct.FatalAnalysisError(
                    " 0x%x is out of %s heap memory !" % (address, memory_type)
                )

            if memory_type == "dm":
                address = freeblock.get_member('u').get_member('next').value
            elif memory_type == "pm":
                address = freeblock.get_member('struct_mem_node').get_member(
                    'u'
                ).get_member('next').value


        free_blocks_info.append(
            "Total heap free : " + cu.mem_size_to_string(total_size, "ow")
        )
        return FreeBlocks(total_size, free_blocks_info)

    def _get_heap_and_magic_offset(self, heap_address, heap_size, memory_type):
        # Get the address we will be working with from the start of heap_pm
        address = heap_address
        if memory_type == "dm":
            heap_data = self.chipdata.get_data(
                heap_address, heap_size
            )
            testblock = self.chipdata.cast(address, 'mem_node')
            testblock_magic = testblock.get_member('u').get_member('magic')
        elif memory_type == "pm":
            heap_data = self.chipdata.get_data_pm(
                heap_address, heap_size
            )
            testblock = self.chipdata.cast(address, 'mem_node_pm', False, 'PM')
            testblock_magic = testblock.get_member(
                'struct_mem_node'
            ).get_member('u').get_member('magic')
        else:
            raise ct.FatalAnalysisError(
                "Memory type %s not recognised." % memory_type
            )

        testblock_address = testblock_magic.address
        # magic_offset here shows the distance between two 32-bit words, first
        # being start of the test block and second being magic value
        magic_offset = (testblock_address - address) // Arch.addr_per_word
        return heap_data, magic_offset

    def _read_alloc_block(self, block_address, memory_type):
        if memory_type == "dm":
            testblock = self.chipdata.cast(block_address, 'mem_node')
            magic = testblock.get_member('u').get_member('magic').value
            # Get length of memory allocation.
            length = testblock.get_member('length').value
            if self.pmalloc_debug_enabled:
                file_address = testblock.get_member('file').value
                line = testblock.get_member('line').value
            else:
                file_address = None
                line = None
        elif memory_type == "pm":
            testblock = self.chipdata.cast(
                block_address, 'mem_node_pm', False, 'PM'
            )
            magic = testblock.get_member('struct_mem_node').get_member(
                'u'
            ).get_member('magic').value
            # Get length of memory allocation.
            length = testblock.get_member('struct_mem_node').get_member(
                'length_32'
            ).value * Arch.addr_per_word
            # Memory debug is not enabled for pm
            file_address = None
            line = None

        if file_address is not None:
            if line == 0:
                # 'file' is actually the value of rlink when we called malloc.
                # We can look that up to work out roughly where the allocation
                # took place.
                try:
                    owner = self.debuginfo.get_source_info(
                        file_address)
                    owner_hint = (
                        owner.src_file + ', near line ' +
                        str(owner.line_number)
                    )
                except ct.BundleMissingError:
                    owner_hint = (
                        "No source information." +
                        "Bundle is missing."
                    )
            else:
                try:
                    filename = self.debuginfo.read_const_string(
                        file_address
                    )
                except KeyError:
                    filename = "Filename cannot be read!"
                except ct.BundleMissingError:
                    filename = (
                        "Filename cannot be read! " +
                        "Bundle is missing."
                    )
                owner_hint = (
                    "{0:s}, line {1:d}".format(filename, line)
                )
        else:
            owner_hint = ""


        return (length, magic, owner_hint)


    def _alloc_blocks(self, heap_address, heap_size, memory_type="dm"):
        """
        @brief Reads and checks the allocated blocks.
        @param[in] self Pointer to the current object
        @param[in] heap_address The heap start address
        @param[in] heap_size The heap size.
        @param[out] Returns two lists. One for the heap
            allocations the other for debug information.
        """
        magic_val = 0xabcd01
        alloc_info = []
        debug_info = []

        (heap_data, magic_offset) = self._get_heap_and_magic_offset(
            heap_address, heap_size, memory_type
        )

        total = 0
        # here index is the index of the magic word in heap_data. Since in
        # heap_pm we are working with 32-bit words and not strictly addresses,
        # for start, index should actually be
        # -Arch.addr_per_word/Arch.arrd_per_word, which is -1
        index = -1

        # Search through the entire heap block, looking for allocated blocks
        # based on the presence of the magic word
        while True:
            try:
                index = index + heap_data[index + 1:].index(magic_val) + 1
                address = heap_address + \
                    (index - magic_offset) * Arch.addr_per_word


                (length, magic, owner_hint) = \
                    self._read_alloc_block(address, memory_type)

                if magic != magic_val:
                    raise ct.AnalysisError(
                        "Magic word not found at expected offset."
                    )

                # Check if we are still in valid region
                if (length > 0) and \
                        (address + length < heap_address + heap_size):
                    alloc_info.append(
                        "Allocated block size : " +
                        cu.mem_size_to_string(length, "o") +
                        " at address: 0x{0:0>8x}".format(address)
                    )
                    index = index + length // Arch.addr_per_word
                    total = total + length
                    if self.pmalloc_debug_enabled:
                        debug_info.append(
                            "Ptr: 0x{0:0>8x} size: ".format(address) +
                            cu.mem_size_to_string(length, "o") +
                            " allocated by: {0:s}".format(owner_hint)
                        )
            except ValueError:
                break

        alloc_info.append(
            "Total heap allocation : " + cu.mem_size_to_string(total, "ow")
        )

        return alloc_info, debug_info

    @abc.abstractmethod
    def _lookup_debuginfo(self):
        """
        @brief Queries debuginfo for information we will need to get the heap
        memory usage.
        @param[in] self Pointer to the current object
        """

    @abc.abstractmethod
    def ret_get_watermarks(self):
        """
        @brief Same as get_watermarks, but it will return values rather than
        print outs.
        @param[in] self Pointer to the current object
        @param[out] Tuple with the heap usage.
        """

    @abc.abstractmethod
    def _get_heap_property(self, heap_number):
        """
        @brief Internal function used to get information about a specific heap.
        @param[in] self Pointer to the current object
        @param[in] heap_number The heap number specifies the heap from which
            information is asked

        @param[out] Tuple containing information about heap.
            (available, heap_size, heap_start, heap_end, heap_free_start)
                available - True, if the heap is present in the build.
                heap_size - Size in octets
                heap_start - Start address
                heap_end - The last valid address
                heap_free_start - The address of the first available block.
        """

    @abc.abstractmethod
    def _get_heap_config(self, processor_number, heap_number):
        """
        @brief In dual core configuration information about the heap can be read
            for the other processor too.
        @param[in] self Pointer to the current object
        @param[in] processor_number The processor where the heap lives.
        @param[in] heap_number The heap number specifies the heap from which
            information is asked

        @param[out] Tuple containing information about heap.
            (available, heap_size, heap_start, heap_end)
                available - True, if the heap is present in the build.
                heap_size - Size in octets
                heap_start - Start address
                heap_end - The last valid address
        """
