/*
 * AudioTeleport
 * Copyright (C) 2023 cair <rui.cai@tenclass.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#pragma warning(disable:26812)
#pragma warning(disable:4200)

inline void DebugPrintf(const char* format, ...) {
  char buf[4096];

  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  va_end(ap);

  OutputDebugStringA(buf);
}

#define AT_UNUSED(x) (void)(x)
#define AT_ERROR(fmt, ...) DebugPrintf("[AudioTeleport] Func Error:%s Line:%d " fmt "\n", __FUNCTION__, __LINE__, __VA_ARGS__)
#define AT_LOG(fmt, ...) DebugPrintf("[AudioTeleport] Func:%s Line:%d " fmt "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#ifdef _DEBUG
#define AT_TAG() DebugPrintf("[AudioTeleport] Func:%s Line:%d \n", __FUNCTION__, __LINE__)
#define AT_ASSERT(condition) assert(condition)
#define AT_PANIC(fmt, ...) do { DebugPrintf("[AudioTeleport] Func Panic:%s Line:%d " fmt "\n", __FUNCTION__, __LINE__, __VA_ARGS__); assert(false); } while (0)

#else
#define AT_TAG()
#define AT_ASSERT(condition)
#define AT_PANIC(fmt, ...)
#endif // _DEBUG

