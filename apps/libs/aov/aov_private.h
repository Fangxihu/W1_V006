/*******************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    aov_private.h

DESCRIPTION
    General AoV data that doesn't need to be shared externally
*/
#ifndef AOV_PRIVATE_H_
#define AOV_PRIVATE_H_

#include <csrtypes.h>
#include <library.h>
#include <message.h>
#include <chain.h>

typedef enum aov_internal_message_type_tag
{
    AOV_INT_MESSAGE_RESET_GRAPH,
    AOV_INT_MESSAGE_SVA_TRIGGERED,
    AOV_INT_MESSAGE_CHECK_NOTIFY_STATE_CHANGE,
    AOV_INT_MESSAGE_TOP
}aov_internal_message_type_t;

extern const TaskData aovInternalTask;

#ifdef HOSTED_TEST_ENVIRONMENT
/*!
    @brief Used for setting internal state of the aov module for testing

    This is only intended for unit test and will panic if called in a release build.
*/
void aovTestResetLib(void);
#endif

/*!
    @brief Used for setting internal state of the aov_graph module for testing

    This is only intended for unit test and will panic if called in a release build.
*/
void aovTestSetGraphData(DataFileID file, kymera_chain_handle_t chain_handle, bool digi_mic);

#endif /* AOV_PRIVATE_H_ */
