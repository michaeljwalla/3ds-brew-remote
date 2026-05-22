#pragma once
#include "server/device_handling/InputMap.h"
#include "server/device_handling/Inputs.h"

InputMap<InputObject>& get_mapping(InputTypes type);