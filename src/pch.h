// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include <g3sdk/SharedBase.h>
#include <g3sdk/Engine.h>
#include <g3sdk/Game.h>
#include <g3sdk/FileSystem.h>
#include <g3sdk/GUI.h>
#include <g3sdk/GFC.h>
#include <g3sdk/Script.h>

#include "framework.h"

extern "C" {
#include "g3mmpinf.h"
}

#include "utils.h"
#include "g3mpfhook.h"
#include "detourmanager.h"
#include "d3doverlay.h"

#endif //PCH_H
