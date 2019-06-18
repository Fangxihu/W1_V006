############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module responsible to analyse the heap memory in Kymera.
"""
from ACAT.Core import CoreTypes as ct
from ACAT.Analysis import DebugLog
from ACAT.Analysis.Heap import Heap



# 'heap_config':() is empty because members are not necessarily accessed,
# 'mem_node' also has members 'line' and 'file' missing since they are in try
VARIABLE_DEPENDENCIES = {
    'strict': (
        '$_processor_heap_info_list', 'L_pheap_info', '$_heap_debug_free',
        '$_heap_debug_min_free'
    )
}
TYPE_DEPENDENCIES = {
    'heap_config': (),
    'heap_info': ()
}

class HeapMem(Heap):
    """
    @brief This class encapsulates an analysis for heap memory usage.
    """
    # heap names
    heap_names = ["DM1 heap", "DM2 heap", "DM2 shared heap", "DM1 ext heap", "DM1 heap+"]
    # maximum number of heaps per processor.
    max_num_heaps = len(heap_names)

    memory_type = "dm"

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Heap.__init__(self, **kwarg)
        # Look up the debuginfo once. Don't do it here though; we don't want
        # to throw an exception from the constructor if something goes
        # wrong.
        self._do_debuginfo_lookup = True
        self.pmalloc_debug_enabled = None
        self.heap_info = None
        self.heap_info_list = None
        self.freelist = None
        self._check_kymera_version()

    def display_configuration(self):
        """
        @brief Prints out the heap configuration for both processors.
        @param[in] self Pointer to the current object
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        self.formatter.section_start('Heap Configuration')
        num_heap_processors = len(self.heap_info_list.members)

        for pnum in range(num_heap_processors):
            self.formatter.section_start('Processor %d' % pnum)
            self.formatter.output(
                self._get_heap_config_str(pnum)
            )
            self.formatter.section_end()
        self.formatter.section_end()

    @DebugLog.suspend_log_decorator(0)
    def ret_get_watermarks(self):
        """
        @brief Same as get_watermarks, but it will return values rather than
        print outs.
        @param[in] self Pointer to the current object
        @param[out] Tuple with the heap usage.
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        total_heap = 0
        free_heap = self.chipdata.get_var_strict("$_heap_debug_free").value
        min_free_heap = self.chipdata.get_var_strict(
            "$_heap_debug_min_free"
        ).value

        for heap_num in range(self.max_num_heaps):
            (available, heap_size, _, _, _) = \
                self._get_heap_property(heap_num)
            if available:
                total_heap += heap_size

        return total_heap, free_heap, min_free_heap


    @DebugLog.suspend_log_decorator(0)
    def clear_watermarks(self):
        """
        @brief Clears the minimum available memory watermark by equating it
        with the current available memory.
        @param[in] self Pointer to the current object
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        free_heap = self.chipdata.get_var_strict("$_heap_debug_free").value
        # Wash down the watermark (min available =  current available)
        self.chipdata.set_data(
            self.chipdata.get_var_strict("$_heap_debug_min_free").address,
            [free_heap]
        )

    ##################################################
    # Private methods
    ##################################################

    def _check_kymera_version(self):
        """
        @brief This function checks if the Kymera version is compatible with
            this analysis. For outdated Kymera OutdatedFwAnalysisError will be
            raised.
        @param[in] self Pointer to the current object
        """
        try:
            self.debuginfo.get_var_strict("$_processor_heap_info_list")
        except ct.DebugInfoNoVariable:
            # fallback to the old implementation
            raise ct.OutdatedFwAnalysisError()

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
        heap_name = self.heap_names[heap_number]

        if heap_name == "DM1 heap+":
            processor_number = self.chipdata.processor
            try:
                # remove this if the memory is re-arranged
                temp_name = "$_heap1_p%d_DM1_addition_start" % processor_number
                heap_start = self.chipdata.get_var_strict(
                    temp_name
                ).value
                temp_name = "$_heap1_p%d_DM1_addition_size" % processor_number
                heap_size = self.chipdata.get_var_strict(
                    temp_name
                ).value
                # "DM1 heap+" has the same free heap list as "DM1 heap".
                # "DM1 heap" is at index
                dm1_heap_index = self.heap_names.index("DM1 heap")
                heap_free_start = self.freelist[dm1_heap_index].value
                available = heap_start != 0
                heap_end = heap_start + heap_size - 1
            except ct.DebugInfoNoVariable:
                heap_start = 0
                heap_size = 0
                heap_free_start = 0
                heap_end = 0
                available = False

        else:
            current_heap = self.heap_info[heap_number]
            heap_size = current_heap.get_member("heap_size").value
            heap_start = current_heap.get_member("heap_start").value
            heap_end = current_heap.get_member("heap_end").value
            heap_end = heap_end - 1
            heap_free_start = self.freelist[heap_number].value
            available = heap_start != 0

        return (available, heap_size, heap_start, heap_end, heap_free_start)

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
        heap_name = self.heap_names[heap_number]
        if heap_name == "DM1 heap+":
            available = False
            heap_start = 0
            heap_end = 0
            heap_size = 0
            try:
                # remove this if the memory is re-arranged
                temp_name = "$_heap1_p%d_DM1_addition_start" % processor_number
                heap_start = self.chipdata.get_var_strict(
                    temp_name
                ).value
                temp_name = "$_heap1_p%d_DM1_addition_end" % processor_number
                heap_end = self.chipdata.get_var_strict(
                    temp_name
                ).value
                temp_name = "$_heap1_p%d_DM1_addition_size" % processor_number
                heap_size = self.chipdata.get_var_strict(
                    temp_name
                ).value

                if heap_start != 0:
                    available = True
            except ct.DebugInfoNoVariable:
                pass
        else:
            proc_config = self.heap_info_list[processor_number].get_member("heap")
            available = proc_config[heap_number].get_member('heap_end').value != 0

            heap_size = proc_config[heap_number].get_member('heap_size').value
            heap_start = proc_config[heap_number].get_member('heap_start').value
            heap_end = proc_config[heap_number].get_member('heap_end').value - 1

        return (available, heap_size, heap_start, heap_end)

    def _lookup_debuginfo(self):
        """
        @brief Queries debuginfo for information we will need to get the heap
        memory usage.
        @param[in] self Pointer to the current object
        """

        if not self._do_debuginfo_lookup:
            return

        self._do_debuginfo_lookup = False

        # Freelist
        self.freelist = self.chipdata.get_var_strict('L_freelist')

        # Check for PMALLOC_DEBUG
        # If L_memory_pool_limits exists then PMALLOC_DEBUG is enabled
        try:
            self.debuginfo.get_var_strict(
                'L_memory_pool_limits'
            )
            self.pmalloc_debug_enabled = True
        except ct.DebugInfoNoVariable:
            self.pmalloc_debug_enabled = False

        pheap_info = self.chipdata.get_var_strict("L_pheap_info").value
        heap_info = self.chipdata.cast(pheap_info, "heap_config")
        self.heap_info = heap_info.get_member("heap")

        # processor_heap_info_list should be always different than NULL!
        self.heap_info_list = self.chipdata.get_var_strict(
            "$_processor_heap_info_list"
        )
