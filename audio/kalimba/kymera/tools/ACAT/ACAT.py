############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
ACAT's main script will import ACAT as a package and run it.
"""
import sys
import os
import imp

def _main():
    # Importing ACAT as a package
    file_dir = os.path.abspath(os.path.dirname(__file__))
    module_name = "ACAT"
    file_handler, filename, desc = imp.find_module(module_name, [file_dir])
    acat_module = imp.load_module(module_name, file_handler, filename, desc)

    return_value = 0
    try:
        # Strip the first element which is the script name.
        parameters = sys.argv[1:]
        acat_module.parse_args(parameters)
        session = acat_module.load_session()
        acat_module.do_analysis(session)
    except acat_module.Core.CoreTypes.Usage as err:
        sys.stderr.write(str(err))
        sys.stderr.write(acat_module.HELP_STRING)
        return_value = 2  # signal the error.

    sys.exit(return_value)


if __name__ == '__main__':
    # call main
    _main()
