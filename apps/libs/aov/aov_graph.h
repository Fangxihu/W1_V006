/*******************************************************************************
Copyright (c) 2017 Qualcomm Technologies International, Ltd.
Part of 6.3.0

FILE NAME
    aov_graph.h

DESCRIPTION
    Functions to configure the AoV graph
*/

#ifndef AOV_GRAPH_H_
#define AOV_GRAPH_H_

#include <file.h>
#include <audio_plugin_if.h>

/******************************************************************************
NAME
    aovCreateGraph

DESCRIPTION
    Creates the AoV graph.

PARAMETERS
    trigger_phrase_file: The model for the phrase to respond to.
    mic_params: The configuration for the microphone to use.

*/
void aovCreateGraph(FILE_INDEX trigger_phrase_file, const audio_mic_params mic_params);

/******************************************************************************
NAME
    aovDestroyGraph

DESCRIPTION
    Tears down the AoV graph
*/
void aovDestroyGraph(void);

/******************************************************************************
NAME
    aovResetGraph

DESCRIPTION
    Resets the AoV graph if it is running.
*/
void aovResetGraph(void);


#endif /* AOV_GRAPH_H_ */
