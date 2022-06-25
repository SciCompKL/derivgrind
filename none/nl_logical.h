#ifndef NL_LOGICAL_H
#define NL_LOGICAL_H

#include "pub_tool_basics.h"

VG_REGPARM(0) UInt nl_logical_and32(UInt x, UInt xd, UInt y, UInt yd);
VG_REGPARM(0) ULong nl_logical_and64(ULong x, ULong xd, ULong y, ULong yd);

VG_REGPARM(0) UInt nl_logical_or32(UInt x, UInt xd, UInt y, UInt yd);
VG_REGPARM(0) ULong nl_logical_or64(ULong x, ULong xd, ULong y, ULong yd);

VG_REGPARM(0) UInt nl_logical_xor32(UInt x, UInt xd, UInt y, UInt yd);
VG_REGPARM(0) ULong nl_logical_xor64(ULong x, ULong xd, ULong y, ULong yd);

#endif // NL_LOGICAL_H
