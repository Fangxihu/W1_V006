############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module to analyse the scratch memory.
"""
from ACAT.Core import CoreTypes as ct
from . import Analysis

VARIABLE_DEPENDENCIES = {'strict': ('L_shared_memory_list',)}
TYPE_DEPENDENCIES = {'SHARED_MEMORY_TABLE': ('next',)}


class SharedMem(Analysis.Analysis):
    """
    @brief This class encapsulates an analysis for usage of shared memory.
    """

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        # Shared memory list
        self._shared_memory_list = self.debuginfo.get_var_strict(
            'L_shared_memory_list'
        )

    def run_all(self):
        """
        @brief Perform all useful analysis and spew the output to the formatter.
        Displays the contents of the shared memory blocks.
        @param[in] self Pointer to the current object
        """
        self.formatter.section_start('Shared Memory Info')
        self._shared_memory_blocks()
        self.formatter.section_end()

    ##################################################
    # Private methods
    ##################################################

    def _shared_memory_blocks(self):
        self._shared_memory_list = self.debuginfo.get_var_strict(
            'L_shared_memory_list'
        ).address
        for sh_table in self.parse_linked_list(
                self._shared_memory_list, 'next'
            ):
            self.formatter.output(str(sh_table))
