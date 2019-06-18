'''
ACAT module version definition
'''

__version__ = '1.7.2'  # pylint: disable=invalid-name

version_info = [  # pylint: disable=invalid-name
    __version__.split('.')[ind]
    if len(__version__.split('.')) > ind else 0 if ind != 3 else ''
    for ind in range(5)
]
