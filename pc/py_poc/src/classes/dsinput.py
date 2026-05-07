from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Callable, Any
from time import perf_counter as tick

# no explicitly hashing these (use is for equality)
# as usually one instantiation gets updated to save memory
class DSInput(ABC):
    def __eq__(self, other): return type(self) == type(other)
    @abstractmethod
    def clone(self):
        pass
#
@dataclass(slots=True, eq=False)
class DSTouchpad(DSInput):
    touch: bool = False
    x: float = 0
    y: float = 0
    
    def clone(self):
        return DSTouchpad(self.touch, self.x, self.y)
