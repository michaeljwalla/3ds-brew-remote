#pragma once
#include <vector>

class InputObject;

InputObject spawn_device();
bool register_device(const InputObject&);
const std::vector<InputObject> get_devices();

