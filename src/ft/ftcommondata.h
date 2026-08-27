#ifndef _FTPROGRAMDATA_H_
#define _FTPROGRAMDATA_H_

// Massive file containing data from various files

#include <ssb_types.h>
#include <sys/obj.h>
#include <ft/fttypes.h>
#include <gm/gmtypes.h>

// 0x8012C830
extern f32 dFTCommonDataHandicapTable[/* */][2];

#ifdef PORT
/* Row count of the above, exported from its definition site because the
 * bound is unspecified here and region-dependent. Valid handicap values
 * are 1..dFTCommonDataHandicapTableCount (the readers index [handicap - 1]). */
extern const s32 dFTCommonDataHandicapTableCount;
#endif

// 0x8012C970
extern u16 dFTCommonDataDownBounceSFX[/* */];

// 0x8012C9A8
extern u16 dFTCommonDataPublicFighterCallFGMs[/* */];

// 0x8012C9E0
extern FTItemThrow dFTCommonDataItemThrowDescs[/* */];

// 0x8012CA38
extern FTItemSwing dFTCommonDataItemSwingAnimSpeeds[/* */];

// 0x8012CA78
extern SYColorRGBA dFTCommonDataShadowColorDefault;

// 0x8012CA7C
extern SYColorRGBA dFTCommonDataShadowColorTeams[/* */];

// 0x8012DBD0
extern GMColDesc dGMColScriptsDescs[/* */]; // 0x8012DBD0

#endif
