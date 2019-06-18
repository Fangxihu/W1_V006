############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Audio Coredump Analysis Tool or ACAT is a pyhton package which is used for
analysing coredumps and live chips running the Kymera firmware.
"""
from .Core import CoreUtils as cu
from ._version import __version__, version_info

# expose all the necesarry functions and globals from core utils.
HELP_STRING = cu.HELP_STRING


def parse_args(parameters):
    """
    See ACAT.CoreUtils.parse_args.
    """
    cu.parse_args(parameters)


def load_session(analyses=None):
    """
    See ACAT.CoreUtils.load_session.
    """
    return cu.load_session(analyses)


def do_analysis(session):
    """
    See ACAT.CoreUtils.do_analysis.
    """
    cu.do_analysis(session)

def under_test():
    """
    ACTA will enter to test mode which makes the following changes in
    the behaviour:
        - Random IDs will be removed from the matplotlib svg output. Note: This
        can cause an erroneous output which cannot be displayed in a browser.
        The reason to do this is to make test reproduceable.

        - Graphviz graphs will be displayed as test. The only way to test
        the Graphviz module is to display the graph as a text because Graphviz
        svg contains different random IDs with shuffled order.

        - Disables error stack printing for all analyses, because the relative
        path to a file is machine dependent.
    """
    cu.global_options.under_test = True

def reset_package():
    """
    Resets every global ACAT variable. Function used for test purposes.
    """
    cu.reset_package()

def set_matplotlib_plotter(plotter_dict):
    """
    Sets the matplotlib plotter. Function used for test purposes.
    """
    cu.set_matplotlib_plotter(plotter_dict)

def create_matplotlib_plotter():
    """
    Creates a dictionary which contains necessary information to manipulate a
    matplotlib plotter. Function used for test purposes.
    """
    return cu.create_matplotlib_plotter()

def destroy_matplotlib_plotter():
    """
    Terminates and destroys the matplotlib plotter.
    Function used for test purposes.
    """
    cu.destroy_matplotlib_plotter()
