#pragma once

#ifdef UNIT_TEST
#define VIRTUAL_FOR_UNIT_TEST virtual
#else
#define VIRTUAL_FOR_UNIT_TEST
#endif

typedef unsigned long TimeMilliseconds_t;
typedef unsigned long TimeMicroseconds_t;