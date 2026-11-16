// pch.h - 预编译头
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstdint>
#include <cstring>
#include <cctype>

#include "TextFmt.h"
#include "PluginIcon.h"

#define TRAFFICE_MONITOR_PLUGIN
#include "PluginInterface.h"
