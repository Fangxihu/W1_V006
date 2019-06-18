/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.3.0
\file       av_headset_log.h
\brief      Header file for the logging marco's

    Two styles of logging are supported.

    They can both be accessed from the pydbg tool using the 
    log commands. The two forms of logging will be displayed
    in the order they were logged. The two styles of logging can
    be combined, but the recommendation is to use the DEBUG_LOG()
    functions.

    Logging can be disabled by use of the define DISABLE_LOG. Defining
    or undefining this at the top of a particular source file would
    allow logging on a module basis.

    The DEBUG_LOG()/DEBUG_LOGF() macros write condensed information 
    to a logging area and can only be decoded if the original 
    application image file (.elf) is available.

    They are however quicker and by virtue of being condensed there
    is less chance of losing information if monitoring information
    in real time.

    The DEBUG_PRINT()/DEBUG_PRINTF() macros write the complete
    string to a different logging area, a character buffer, and 
    can be decoded even if the application image file is not available.

    The use of DEBUG_PRINT() is not recommended apart from, for instance,
    printing the contents of a message or buffer. Due to memory
    constraints the available printf buffer is relatively small
    and information can be lost. The DEBUG_PRINTF()functions can also 
    use a lot of the software stack which can cause a Panic() due to
    a stack overflow.
*/


#ifndef AV_HEADSET_LOG_H
#define AV_HEADSET_LOG_H

#include <hydra_log.h>



/*! \cond internals 
    This is some cunning macro magic that is able to count the
    number of arguments supplied to DEBUG_LOGF
 */

#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

extern void hydra_log_firm_variadic(const char *event_key, size_t n_args, ...);
/*! \endcond internals */

extern uint16 globalDebugLineCount;

#ifndef DISABLE_LOG
/*! \brief  Display the supplied string in the condensed log.

    This version of the DEBUG_LOG() macro can accept additional
    parameters, much like printf().

    Like printf() the fmt string can include %% characters. 
    Due to the implementation of logging there is a limit of 
    4 parameters.

    \param  fmt Format string for debug. May use %% formating in the
        same way as printf()
 */
#define DEBUG_LOGF(fmt, ...) \
    do { \
        HYDRA_LOG_STRING(log_fmt, "%04X: " fmt); \
        hydra_log_firm_variadic(log_fmt, VA_NARGS(__VA_ARGS__) + 1, globalDebugLineCount++, __VA_ARGS__); \
    } while (0)

/*! \brief  Display the supplied string in the condensed log

    \param fmt  String to display in the log
 */
#define DEBUG_LOG(fmt) \
    do { \
        HYDRA_LOG_STRING(log_fmt, "%04X: " fmt); \
        hydra_log_firm_1(log_fmt, globalDebugLineCount++); \
    } while (0)

/*! \brief  Include a string formatted by printf and with 
        parameters in the character debug buffer

    There is no restriction on the number of parameters that 
    can be supplied.

    See the description in the file as to why the use of this
    function is only recommended for some limited debug.

    \param  fmt Format string for debug. May use %% formating in the
        same way as printf()
 */
#define DEBUG_PRINTF(fmt, ...) \
        printf(fmt, __VA_ARGS__)

/*! \brief  Include a string, without parameters in the 
        character debug buffer

    See the description in the file as to why the use of this
    function is not recommended.

    \param fmt  String to display in the log
 */
#define DEBUG_PRINT(fmt) \
        printf(fmt)
#else
#define DEBUG_LOGF(fmt, ...)
#define DEBUG_LOG(fmt)
#define DEBUG_PRINTF(fmt, ...)
#define DEBUG_PRINT(fmt)
#endif

#endif // AV_HEADSET_LOG_H
