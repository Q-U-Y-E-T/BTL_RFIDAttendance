/* Host-only: Linux LP64 long is 64-bit; FatFs R0.11 requires DWORD=32-bit.
 * Force-include this adapter for host tests; vendor integer.h stays unchanged.
 */
#ifndef _FF_INTEGER
#define _FF_INTEGER
#include <stdint.h>
typedef uint8_t BYTE;
typedef int16_t SHORT;
typedef uint16_t WORD;
typedef uint16_t WCHAR;
typedef int INT;
typedef unsigned int UINT;
typedef int32_t LONG;
typedef uint32_t DWORD;
#endif
