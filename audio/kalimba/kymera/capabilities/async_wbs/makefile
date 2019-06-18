############################################################################
# CONFIDENTIAL
# Project Kymera
# Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.
# Unit 400, Cambridge Science Park,
# Milton Road, Cambridge, CB4 0WH.  UK
# Tel: +44 1223 692000  Fax: +44 1223 692001
#
############################################################################
#
# COMPONENT:    makefile
# MODULE:       wbs
# DESCRIPTION:  Library makefile.
#
############################################################################


#########################################################################
# Define root directory (relative so we can be installed anywhere)
#########################################################################

BUILD_ROOT = ../../build
include $(BUILD_ROOT)/roots.mkf

#########################################################################
# Enter source files and directories and header directories here.
#
# Makerules will add the standard interface paths
#########################################################################

C_SRC += async_wbs_dec.c
C_SRC += async_wbs_enc.c
C_SRC += async_wbs.c


# Include Sco common functions
H_PATH += ${KYMERA_ROOT}/capabilities/sco_fw
# Include Wbs functions
H_PATH += ${KYMERA_ROOT}/capabilities/wbs

#PATCH_SRC = wbs_c_stubs.asm

#GEN_PIDS = $(PATCH_DIR)/capability_wbs_patch_ids.txt

#CPS_XML = async_wbs_dec.xml

#########################################################################
# Enter final target file here (only 1 target should be specified)
#########################################################################

TARGET_EXE_ROOT =
TARGET_LIB_ROOT = async_wbs

#########################################################################
# Include the standard definitions and rules
#########################################################################

include $(BUILD_ROOT)/makerules_cps.mkf
include $(BUILD_ROOT)/makerules.mkf

