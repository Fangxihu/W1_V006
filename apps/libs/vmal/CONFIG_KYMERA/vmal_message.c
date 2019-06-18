/*******************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.

FILE NAME
    vmal_message.c

DESCRIPTION
    Operator Sources do not have an associated Sink, therefore separate traps
    are required to access the Message Task
*/

#include <vmal.h>
#include <stream.h>

Task VmalMessageSinkTask(Sink sink, Task task)
{
    return MessageStreamTaskFromSink(sink, task);
}

Task VmalMessageSinkGetTask(Sink sink)
{
    return MessageStreamGetTaskFromSink(sink);
}

Task VmalMessageSourceTask(Source source, Task task)
{
    return MessageStreamTaskFromSource(source, task);
}

Task VmalMessageSourceGetTask(Source source)
{
    return MessageStreamGetTaskFromSource(source);
}
