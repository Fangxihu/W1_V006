############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module to connect to an SPI or trb port.
"""
import re
import time

from ACAT.Core import LiveChip
from ACAT.Core import Arch
from ACAT.Core import CoreUtils as cu

##################################################


class LiveSpi(LiveChip.LiveChip):
    """
    @brief Provide access to chip data on a live chip over SPI
    """

    def __init__(self, kal, spi_trans, processor, wait_for_proc_to_start):
        """
        @brief Constructor

        Starts a spi connection to a chip
        @param[in] self Pointer to the current object
        @param[in] kal
        @param[in] spi_trans
        @param[in] processor
        """
        LiveChip.LiveChip.__init__(self)

        self.kal = kal
        self.dm = kal.dm

        # Throttle the memory read to avoid audio stalls. This is a workaround
        # to avoid the exhaustion of the transaction bridge.
        old_read_dm_block = self.dm.__dict__['_reader']
        def throttled_read_dm_block(address, words_to_fetch):
            """
            This functions slow the memory read by fragmeting the memory read
            to "maximum_read" words.
            """
            # Adjust this if the reads are still too fast.
            maximum_read = 32
            if words_to_fetch < maximum_read:
                return old_read_dm_block(address, words_to_fetch)

            retval = []
            for _ in range(words_to_fetch / maximum_read):
                retval += old_read_dm_block(address, maximum_read)
                address += maximum_read * Arch.addr_per_word
                words_to_fetch -= maximum_read

            if words_to_fetch:
                # read the remaining words
                retval += old_read_dm_block(address, words_to_fetch)
            return retval

        self.dm.__dict__['_reader'] = throttled_read_dm_block

        self.pm = kal.pm
        self.processor = processor

        version = ""
        try:
            version = kal.get_matching_kalimbalab()
        except AttributeError:
            # 24 or older Kalimbalabs does not have get_matching_kalimbalab methods
            # Fist element form the list is the kal version.
            version = "pre 24b " + kal.get_version()[0]
        cu.debug_log("Initialising connection with KalAccess %s" % version)

        # Try and connect without providing a subsys or processor ID.
        # This should Just Work on Bluecore chips, and Hydra chips if using a
        # version of KalimbaLab older than 22a. If it doesn't work, things
        # get a bit harder.

        # Support connect_with_uri transport format
        if spi_trans.startswith("device://"):
            spi_trans += "/audio/p{}".format(str(processor))
            connect_func = self.kal.connect_with_uri
        else:
            connect_func = self.kal.connect
        try:
            connect_func(spi_trans)
        except Exception as exception:
            if re.search("Could not connect", str(exception)):
                raise
            else:
                # Assume a compatibility problem and try again.
                # Supply defaults for mul(-1) and ignore_fw (0), we don't care about them.
                # Audio is always subsystem 3 on a Hydra chip, now and for the
                # forseeable future.
                connect_func(spi_trans, -1, 0, 3, processor)

        if wait_for_proc_to_start:
            # Wait until the chip is booted up otherwise ACAT will fail anyways.
            # Just checking if the processor is running is not enough to tell
            # if the processor was booted. However, reading form the PM ram
            # region gives error if the chip wasn't booted.
            message_displayed = False
            while True:
                try:
                    self.pm[0]
                    break  # proc booted exit now
                except BaseException:
                    if not message_displayed:
                        print("Waiting for the audio subsystem to boot...")
                        message_displayed = True
                    time.sleep(0.1)
            print("Audio subsystem booted.")

        # If we get here, we must have connected ok. We need to call chip_select to
        # pick the correct memory map for the chip we're attached to.
        try:
            if self.kal.arch.is_hydra():
                chip_arch = "Hydra"
            # KaArch.is_bluecore() only exists from KalimaLab23 onwards.
            elif self.kal.arch.is_bluecore():
                chip_arch = "Bluecore"
            else:
                # Raise StandardError to deal with chips that are no
                # hydra/Bluecore family.
                raise Exception
        except AttributeError:
            raise Exception("Could not detect the arch or chip name")
        except Exception:
            # Dealing with non-Hydra/Bluecore chips.
            if self.kal.arch.get_chip_name() == "marco":
                chip_arch = "KAS"
            # Napier audio added on kal-python tools 1.1.2
            elif self.kal.arch.get_chip_name() == "napier_audio":
                chip_arch = "Napier"
            else:
                raise Exception("Could not detect the arch or chip name")

        # From KalimaLab23 onwards, we can read the chip ID. If we can't get it, just pass in
        # zero so that any compatibility issue becomes obvious.
        try:
            Arch.chip_select(
                self.kal.arch.get_arch(), chip_arch,
                self.kal.arch.get_global_chip_version(),
                self._get_chip_revision()
            )
        except AttributeError:
            Arch.chip_select(self.kal.arch.get_arch(), chip_arch, 0)


############################
# Private methods
############################

    def _get_chip_revision(self):
        """
        @brief Returns the chip revision number from the SLT.
        @param[in] kal Instance of kalaccess
        @param[in] addr_per_word
        """
        try:
            addr_per_word = self.kal.dm.addresses_per_word()
            slt_ptr = self.kal._memory.read_pm_block(
                Arch.pRegions_Crescendo['SLT'][0] + addr_per_word, 1
            )
            # first 7 entries
            slt_entries = self.kal._memory.read_dm_block(slt_ptr[0], 14)

            # the address of register that holds the chip version is at entry 4
            # currently, the chip revision is only used to differentiate
            # between d00 and d01 Crescendo digits
            # for different platforms, entry 4 might not even exist or the chip
            # revision might be at a different entry
            for i, entry in enumerate(slt_entries):
                if entry == 4:
                    # chip version address
                    chip_version_addr = slt_entries[i + 1]

            chip_revision = self.kal._memory.read_dm_block(
                chip_version_addr, 1
            )[0] >> 8
            return chip_revision
        except BaseException:
            return None
