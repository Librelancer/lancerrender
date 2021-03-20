#ifndef _LR_STRING_H_
#define _LR_STRING_H_

#include <string.h>

#ifdef _MSC_VER
#define lr_strdup _strdup
#else
#define lr_strdup strdup
#endif

#endif