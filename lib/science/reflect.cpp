/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 *
 * This file is not built under all configs. It is only built if
 * "use_spirv_cross_reflection" is enabled, since it adds a
 * dependency on spirv_cross.
 */
#include <vendor/spirv_cross/spirv_glsl.hpp>
#include "science.h"

#ifndef USE_SPIRV_CROSS_REFLECTION
#error USE_SPIRV_CROSS_REFLECTION should be defined if this file is being built.
#endif
