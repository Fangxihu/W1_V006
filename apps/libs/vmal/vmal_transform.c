/*******************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.

FILE NAME
    vmal_transform.c

DESCRIPTION
    Create shim version of transform traps
*/

#include <vmal.h>
#include <panic.h>

Transform VmalTransformRtpSbcDecode(Source source, Sink sink)
{
    Transform t = TransformRtpDecode(source, sink);
    return t;
}

Transform VmalTransformRtpSbcEncode(Source source, Sink sink)
{
    Transform t = TransformRtpEncode(source, sink);
    return t;
}

Transform VmalTransformRtpMp3Decode(Source source, Sink sink)
{
    Transform t = TransformRtpDecode(source, sink);
    return t;
}

Transform VmalTransformRtpMp3Encode(Source source, Sink sink)
{
    Transform t = TransformRtpEncode(source, sink);
    return t;
}

Transform VmalTransformRtpAacDecode(Source source, Sink sink)
{
    Transform t = TransformRtpDecode(source, sink);
    return t;
}
