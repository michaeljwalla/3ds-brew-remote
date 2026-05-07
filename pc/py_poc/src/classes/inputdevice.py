from dataclasses import dataclass, field
from .dsinput import *

@dataclass(slots=True, eq=False)
class InputBinding:
    input: DSInput
    callback: Callable # args i: DSInput, dt: float (s). first dt is always 0
    enabled: bool = False
    _lasttick: int = 0

    def __hash__(self):
        return hash(type(self.input), self.callback) # because DSInput might be cloned, but callback should always be by reference
    #
    def update(self):
        self.callback(self.input, 0 if not _lasttick else tick() - _lasttick) 
        _lasttick = tick()

@dataclass(slots=True)
class InputDevice:
    name: str = "InputDevice"
    registers: dict[type[DSInput], set[InputBinding]] = field(default_factory=dict)

    def add(self, i: InputBinding):
        t = type(i.input)
        if t not in self.registers:
            self.registers[t] = set(i)
            return
        #
        register = self.registers[t]
        if i in register: return
        #
        register.add(i)
        return
    
    def remove(self, i:InputBinding):
        t = type(i.input)
        if t not in self.registers:
            return
        #
        register = self.registers[t]
        if i not in register: return
        #
        register.remove(i)
        return

