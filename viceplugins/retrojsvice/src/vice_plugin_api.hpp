#pragma once

#ifdef _WIN32
#define VICE_PLUGIN_API_FUNC_DECLSPEC __declspec(dllexport)
#endif

#include "../../../vice_plugin_api.h"
