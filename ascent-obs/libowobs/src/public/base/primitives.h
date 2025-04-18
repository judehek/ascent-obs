/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef LIBASCENTOBS_BASE_PRIMITIVES_H_
#define LIBASCENTOBS_BASE_PRIMITIVES_H_

#ifdef _MSC_VER
#ifndef _STDINT
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
#endif
#else
#include <stdint.h>
#endif

#endif // LIBASCENTOBS_BASE_PRIMITIVES_H_