from abc import ABCMeta
from enum import Enum, EnumMeta

class DSInput(ABCMeta, EnumMeta):
    pass

class DSTouch(DSInput, Enum):
    pass
