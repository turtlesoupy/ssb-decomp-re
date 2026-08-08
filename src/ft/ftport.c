/**
 * ftport.c - decomp-side helpers for the port fighter registry.
 *
 * Lives in decomp/ because it needs access to the source-of-truth
 * vanilla arrays (dFTManagerDataFiles[] / dFTCommonSpecial*StatusList[] /
 * dFTCommonEntryAppearStatusIDs[] / ...) and to the per-character efManager
 * entry-effect functions called from the original ftCommonAppearSetStatus
 * switch. The port-side .cpp can't pull these headers in without dragging
 * the decomp shim headers across the C++ boundary.
 *
 * Two exports:
 *   port_fighter_seed_vanilla() - registers fkinds 0..26 from the vanilla
 *       decomp tables. Called once at PortInit.
 *   ftPortVanillaEntryMakeEffect() - the per-fkind appear-effect switch
 *       previously inlined in ftCommonAppearSetStatus. Registered as the
 *       entry_make_effect callback for every vanilla fkind during seeding.
 */

#ifdef PORT

#include <ft/fighter.h>
#include <ef/efmanager.h>
#include <sys/objman.h>          /* gGCCommonLinks */
#include <sys/objdef.h>          /* nGCCommonLinkIDFighter */
#include <sc/scsubsys/scsubsys.h>

#include "fighter_registry.h"

/* Per-character status descs (used as the ft_data row's special_descs). */
extern FTStatusDesc *dFTMainSpecialStatusDescs[];

/* Special-enter dispatch tables (one function per fkind). */
extern void (*dFTCommonSpecialNStatusList[])(GObj *);
extern void (*dFTCommonSpecialHiStatusList[])(GObj *);
extern void (*dFTCommonSpecialLwStatusList[])(GObj *);
extern void (*dFTCommonSpecialAirNStatusList[])(GObj *);
extern void (*dFTCommonSpecialAirHiStatusList[])(GObj *);
extern void (*dFTCommonSpecialAirLwStatusList[])(GObj *);

/* Match-entry status IDs (one [R,L] pair per fkind). */
extern s32 dFTCommonEntryAppearStatusIDs[][2];

/* Per-fkind costume rows. */
extern FTCostume dFTParamCostumeIDs[];

/* Per-fkind float scale. */
extern f32 dSCSubsysFighterScales[];

/* Per-fkind base color-anim ID (damage_level offset added at read). */
extern s32 dFTParamSkeletonColAnimIDs[];

/* Per-fkind yoshi-egg damage collision descriptors. */
extern ftCommonYoshiEggDesc dFTCommonYoshiEggDamageCollDescs[];

/* Per-fkind down-bounce SFX IDs (u16). */
extern u16 dFTCommonDataDownBounceSFX[];

/* Per-fkind public-call FGM IDs (u16). */
extern u16 dFTCommonDataPublicFighterCallFGMs[];

/* Per-fkind CPU attack-list pointers. */
extern FTComputerAttack *dFTComputerAttackList[];

/* CSS attack1 motion descs - per-fkind row of 8 entries. */
extern void *dMNCharactersAttack1MotionDescs[][8];

#define PORT_VANILLA_FKIND_COUNT  27

/* Mirrors the per-fkind appear-effect switch that used to live inline in
 * ftCommonAppearSetStatus (decomp/src/ft/ftcommon/ftcommonentry.c). */
void ftPortVanillaEntryMakeEffect(FTStruct *fp)
{
    GObj *boss_target_gobj;

    switch (fp->fkind)
    {
    case nFTKindMario:
    case nFTKindLuigi:
    case nFTKindMMario:
        efManagerMarioEntryDokanMakeEffect(&fp->entry_pos, fp->fkind);
        break;

    case nFTKindFox:
        efManagerFoxEntryArwingMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    case nFTKindDonkey:
    case nFTKindGDonkey:
        efManagerDonkeyEntryTaruMakeEffect(&fp->entry_pos);
        break;

    case nFTKindSamus:
        efManagerSamusEntryPointMakeEffect(&fp->entry_pos);
        break;

    case nFTKindLink:
        efManagerLinkEntryWaveMakeEffect(&fp->entry_pos);
        efManagerLinkEntryBeamMakeEffect(&fp->entry_pos);
        break;

    case nFTKindYoshi:
        efManagerYoshiEntryEggMakeEffect(&fp->entry_pos);
        break;

    case nFTKindKirby:
        efManagerKirbyEntryStarMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    case nFTKindPikachu:
    case nFTKindPurin:
        efManagerMBallThrownMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    case nFTKindCaptain:
        if (fp->status_vars.common.entry.lr == -1)
        {
            fp->status_vars.common.entry.is_rotate = TRUE;
        }
        efManagerCaptainEntryCarMakeEffect(&fp->entry_pos, fp->status_vars.common.entry.lr);
        break;

    case nFTKindBoss:
        /* Master Hand picks its target from the live fighter link list
         * (first non-self entry) and stashes it on the boss passive
         * state. No visual effect. */
        boss_target_gobj = gGCCommonLinks[nGCCommonLinkIDFighter];

        while (boss_target_gobj != NULL)
        {
            if (boss_target_gobj != fp->fighter_gobj)
            {
                break;
            }
            else boss_target_gobj = boss_target_gobj->link_next;
        }
        fp->passive_vars.boss.p->target_gobj = boss_target_gobj;

        break;

    default:
        break;
    }
}

/* Seed slots 0..26 of the registry from the vanilla decomp arrays.
 * Mods may overwrite individual rows later; for safety, Mario (fkind 0)
 * is the fallback row consulted by accessors when a synth row left a
 * field NULL. */
void port_fighter_seed_vanilla(void)
{
    s32 fkind;
    FighterDescriptor desc;

    for (fkind = 0; fkind < PORT_VANILLA_FKIND_COUNT; fkind++)
    {
        /* Zero-init every field so untracked fields stay deterministic. */
        FTData *zero_data = NULL;
        s32 i;
        u8 *p = (u8 *)&desc;
        for (i = 0; i < (s32)sizeof(desc); i++) p[i] = 0;
        (void)zero_data;

        desc.ft_data                            = dFTManagerDataFiles[fkind];
        desc.special_descs                      = dFTMainSpecialStatusDescs[fkind];
        desc.special_descs_count                = 0;  /* vanilla never bounds-checks; synth fills this */

        desc.special_handler[PORT_FIGHTER_SPECIAL_N]      = dFTCommonSpecialNStatusList[fkind];
        desc.special_handler[PORT_FIGHTER_SPECIAL_HI]     = dFTCommonSpecialHiStatusList[fkind];
        desc.special_handler[PORT_FIGHTER_SPECIAL_LW]     = dFTCommonSpecialLwStatusList[fkind];
        desc.special_handler[PORT_FIGHTER_SPECIAL_AIR_N]  = dFTCommonSpecialAirNStatusList[fkind];
        desc.special_handler[PORT_FIGHTER_SPECIAL_AIR_HI] = dFTCommonSpecialAirHiStatusList[fkind];
        desc.special_handler[PORT_FIGHTER_SPECIAL_AIR_LW] = dFTCommonSpecialAirLwStatusList[fkind];

        desc.entry_appear_status[0] = dFTCommonEntryAppearStatusIDs[fkind][0];
        desc.entry_appear_status[1] = dFTCommonEntryAppearStatusIDs[fkind][1];
        desc.entry_make_effect      = ftPortVanillaEntryMakeEffect;

        /* D_ovl1_80390D20 is `FTOpeningDesc *[]`; vanilla code reads
         * D_ovl1_80390D20[fkind] and gets a FTOpeningDesc *. Store that
         * pointer directly so the accessor's return matches the original
         * read site one-for-one. */
        desc.opening_descs          = D_ovl1_80390D20[fkind];

        desc.costume_row            = &dFTParamCostumeIDs[fkind];
        /* dSCSubsysFighterScales has rows for the 12 playable fkinds only;
         * vanilla read sites never index it beyond nFTKindPlayableEnd. Give
         * boss/metal/polygon/giant rows a neutral 1.0 instead of reading
         * past the table. */
        desc.scale                  = (fkind <= nFTKindPlayableEnd) ? dSCSubsysFighterScales[fkind] : 1.0f;
        desc.skeleton_col_anim_base = dFTParamSkeletonColAnimIDs[fkind];
        desc.yoshi_egg_damage_coll  = &dFTCommonYoshiEggDamageCollDescs[fkind];
        desc.down_bounce_fgm        = (s32)dFTCommonDataDownBounceSFX[fkind];
        desc.public_call_fgm        = (s32)dFTCommonDataPublicFighterCallFGMs[fkind];
        desc.computer_attack_list   = dFTComputerAttackList[fkind];

        /* CSS fields seeded from mncharacters tables; specific motion id
         * lives elsewhere and is set per-character by the menu code at
         * CSS time, so the registry row just carries the per-fkind row
         * pointer for now. */
        desc.css_motion_special     = 0;
        desc.css_attack1_motion_descs = &dMNCharactersAttack1MotionDescs[fkind][0];

        /* SR engine extensions stay at NULL/0 for vanilla fkinds so the
         * accessors return the "no override" sentinel. team_costume[] is
         * a special case: 0x00 is a valid costume index, so we explicitly
         * set 0xFF (= "no team-costume override") for vanilla rows. */
        desc.team_costume[0] = 0xFFu;
        desc.team_costume[1] = 0xFFu;
        desc.team_costume[2] = 0xFFu;
        desc.team_costume[3] = 0xFFu;

        port_fighter_register(fkind, &desc);
    }
}

#endif /* PORT */

#ifdef PORT
/* ========================================================================= */
/*  OpenSmash pipeline: skeleton dump (SSB64_DUMP_SKELETON=<fkind>)          */
/*                                                                           */
/*  Emits one SKELDUMP line per joint: index, parent joint index, rest       */
/*  world position, local translate, and DL pointer. The offline mesh        */
/*  converter segments a generated T-pose mesh against this skeleton.       */
/* ========================================================================= */

extern void gmCollisionGetFighterPartsWorldPosition(DObj *main_dobj, Vec3f *vec);
extern void port_log(const char *fmt, ...);

void port_dump_skeleton(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    s32 i, p;

    if (fp == NULL)
    {
        port_log("SKELDUMP: no FTStruct\n");
        return;
    }

    port_log("SKELDUMP: begin fkind=%d costume=%d\n", (int)fp->fkind, (int)fp->costume);

    for (i = 0; i < FTPARTS_JOINT_NUM_MAX; i++)
    {
        DObj *j = fp->joints[i];
        s32 parent_idx = -1;
        Vec3f world;

        if (j == NULL)
        {
            continue;
        }

        if (j->parent != NULL)
        {
            for (p = 0; p < FTPARTS_JOINT_NUM_MAX; p++)
            {
                if (fp->joints[p] == j->parent)
                {
                    parent_idx = p;
                    break;
                }
            }
        }

        world.x = world.y = world.z = 0.0f;
        gmCollisionGetFighterPartsWorldPosition(j, &world);

        port_log("SKELDUMP: joint=%d parent=%d world=(%.3f,%.3f,%.3f) local=(%.3f,%.3f,%.3f) dl=%p flags=0x%02x\n",
                 (int)i, (int)parent_idx,
                 world.x, world.y, world.z,
                 j->translate.vec.f.x, j->translate.vec.f.y, j->translate.vec.f.z,
                 (void *)j->dv, (unsigned)j->flags);
    }

    port_log("SKELDUMP: end fkind=%d\n", (int)fp->fkind);
}
#endif /* PORT */

#ifdef PORT
/* ========================================================================= */
/*  OpenSmash pipeline: runtime mesh injection (SSB64_INJECT_BUNDLE)         */
/*                                                                           */
/*  Loads an .osb v2 bundle (pipeline/convert_glb.py write_binary) and       */
/*  replaces the fighter's per-joint display lists with runtime-built,      */
/*  Gouraud-shaded vertex-colored geometry. Bundle triangles arrive          */
/*  pre-batched into <=30-unique-vertex windows, so no triangle is ever      */
/*  dropped at DL build time. Gated on SSB64_INJECT_BUNDLE=<path> and        */
/*  SSB64_INJECT_FKIND (default 0 = Mario).                                  */
/* ========================================================================= */

#include <PR/gbi.h>
#include <stdio.h>

extern void *malloc(size_t);
extern void free(void *);
extern char *getenv(const char *);
extern int atoi(const char *);

typedef struct OSBVert { s16 x, y, z; u8 r, g, b, pad; } OSBVert;

static Gfx *osbBuildPartDL(FILE *f, u32 nbatches)
{
    Gfx *dl;
    Gfx *g;
    Vtx *vtx_all;
    u32 total_v = 0, total_t = 0, b, voff;
    long part_start = ftell(f);

    /* Pass 1: size the allocations from the batch headers. */
    for (b = 0; b < nbatches; b++)
    {
        u32 hdr[2];
        if (fread(hdr, 4, 2, f) != 2) return NULL;
        total_v += hdr[0];
        total_t += hdr[1];
        fseek(f, (long)(hdr[0] * sizeof(OSBVert) + hdr[1] * 4), SEEK_CUR);
    }
    fseek(f, part_start, SEEK_SET);

    vtx_all = (Vtx *)malloc(sizeof(Vtx) * total_v);
    dl = (Gfx *)malloc(sizeof(Gfx) * (8 + nbatches + total_t + 2));
    if (vtx_all == NULL || dl == NULL)
    {
        return NULL;
    }
    g = dl;

    gDPPipeSync(g++);
    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(g++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPClearGeometryMode(g++, G_LIGHTING | G_TEXTURE_GEN | G_CULL_BOTH);
    gSPSetGeometryMode(g++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);

    voff = 0;
    for (b = 0; b < nbatches; b++)
    {
        u32 hdr[2];
        u32 i;
        OSBVert vraw[30];
        u8 traw[4 * 512];

        fread(hdr, 4, 2, f);
        fread(vraw, sizeof(OSBVert), hdr[0], f);
        fread(traw, 4, hdr[1], f);

        for (i = 0; i < hdr[0]; i++)
        {
            Vtx *v = &vtx_all[voff + i];
            v->v.ob[0] = vraw[i].x;
            v->v.ob[1] = vraw[i].y;
            v->v.ob[2] = vraw[i].z;
            v->v.flag = 0;
            v->v.tc[0] = 0;
            v->v.tc[1] = 0;
            v->v.cn[0] = vraw[i].r;
            v->v.cn[1] = vraw[i].g;
            v->v.cn[2] = vraw[i].b;
            v->v.cn[3] = 0xFF;
        }
        gSPVertex(g++, &vtx_all[voff], hdr[0], 0);
        for (i = 0; i < hdr[1]; i++)
        {
            gSP1Triangle(g++, traw[i * 4 + 0], traw[i * 4 + 1], traw[i * 4 + 2], 0);
        }
        voff += hdr[0];
    }

    gDPPipeSync(g++);
    gSPEndDisplayList(g++);
    return dl;
}

void port_inject_bundle(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    const char *path = getenv("SSB64_INJECT_BUNDLE");
    const char *fk_env = getenv("SSB64_INJECT_FKIND");
    int want_fkind = (fk_env != NULL) ? atoi(fk_env) : 0;
    FILE *f;
    char magic[4];
    u32 nparts, p;
    u32 replaced = 0;

    if (path == NULL || fp == NULL || (int)fp->fkind != want_fkind)
    {
        return;
    }

    f = fopen(path, "rb");
    if (f == NULL)
    {
        port_log("OSB: cannot open %s\n", path);
        return;
    }
    fread(magic, 1, 4, f);
    if (magic[0] != 'O' || magic[1] != 'S' || magic[2] != 'B' || magic[3] != '2')
    {
        port_log("OSB: bad magic in %s (want OSB2)\n", path);
        fclose(f);
        return;
    }
    fread(&nparts, 4, 1, f);
    port_log("OSB: injecting %s (%d parts) into fkind=%d\n", path, (int)nparts, (int)fp->fkind);

    for (p = 0; p < nparts; p++)
    {
        u32 hdr[2];
        Gfx *dl;

        fread(hdr, 4, 2, f);
        dl = osbBuildPartDL(f, hdr[1]);
        if (dl != NULL && hdr[0] < FTPARTS_JOINT_NUM_MAX && fp->joints[hdr[0]] != NULL)
        {
            fp->joints[hdr[0]]->dl = dl;
            replaced++;
        }
    }
    fclose(f);
    port_log("OSB: replaced %d joint DLs\n", (int)replaced);
}
#endif /* PORT */
