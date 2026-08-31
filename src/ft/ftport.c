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

        /* v2: full world frame via the same walker — transform the three
         * basis points and subtract the origin to get world basis rows. */
        {
            Vec3f bx, by, bz;
            bx.x = 1.0f; bx.y = 0.0f; bx.z = 0.0f;
            by.x = 0.0f; by.y = 1.0f; by.z = 0.0f;
            bz.x = 0.0f; bz.y = 0.0f; bz.z = 1.0f;
            gmCollisionGetFighterPartsWorldPosition(j, &bx);
            gmCollisionGetFighterPartsWorldPosition(j, &by);
            gmCollisionGetFighterPartsWorldPosition(j, &bz);
            port_log("SKELDUMP2: joint=%d o=(%.4f,%.4f,%.4f) x=(%.4f,%.4f,%.4f) y=(%.4f,%.4f,%.4f) z=(%.4f,%.4f,%.4f)\n",
                     (int)i,
                     world.x, world.y, world.z,
                     bx.x - world.x, bx.y - world.y, bx.z - world.z,
                     by.x - world.x, by.y - world.y, by.z - world.z,
                     bz.x - world.x, bz.y - world.y, bz.z - world.z);
        }
    }

    port_log("SKELDUMP: end fkind=%d\n", (int)fp->fkind);

    /* ---- vanilla part-mesh dump: walk each joint's F3DEX2 DL, collect
     * G_VTX vertex arrays, and log joint-local positions. Drives the
     * offline converter's per-part proportion conforming (the generated
     * mesh's parts are rescaled to the vanilla part bounds). Embedded
     * pointers inside loaded model data are u32 relocation tokens —
     * resolve via the port reloc table; host heap pointers (top 32 bits
     * set) pass through. */
    {
        extern void *portRelocResolvePointerDebug(unsigned int token, const char *file, int line);

        for (i = 0; i < FTPARTS_JOINT_NUM_MAX; i++)
        {
            DObj *j = fp->joints[i];
            Gfx *stack[8];
            int sp = 0;
            Gfx *g;
            int guard = 0;
            int nv_total = 0;
            int bailed = 0;

            if (j == NULL || j->dv == NULL)
            {
                continue;
            }
            /* The loaded model DLs are raw N64 8-byte commands (u32 word
             * pairs, already byte-swapped to host LE). Embedded addresses
             * are port reloc TOKENS (0x00100883-style) — resolve them;
             * anything the resolver rejects (e.g. dynamic segment-0xE
             * heap refs) is skipped rather than followed. */
            {
                const u32 *w = (const u32 *)j->dv;
                const u32 *wstack[8];

                port_log("MESHWALK: joint=%d dl=%p\n", (int)i, (void *)w);

                while (w != NULL && guard++ < 20000)
                {
                    u32 w0 = w[0], w1 = w[1];
                    u32 op = w0 >> 24;

                    if (op == 0xDF) /* G_ENDDL */
                    {
                        if (sp > 0) { w = wstack[--sp]; continue; }
                        break;
                    }
                    else if (op == 0xDE) /* G_DL */
                    {
                        u32 nopush = (w0 >> 16) & 0xFF;
                        const u32 *tgt =
                            (const u32 *)portRelocResolvePointerDebug(w1, "ftport-meshdump-dl", 0);
                        if (tgt != NULL)
                        {
                            if (nopush == 0 && sp < 8)
                            {
                                wstack[sp++] = w + 2;
                            }
                            w = tgt;
                            continue;
                        }
                        /* unresolvable (dynamic segment ref) — skip */
                    }
                    else if (op == 0x01) /* G_VTX (F3DEX2) */
                    {
                        u32 n = (w0 >> 12) & 0xFF;
                        const Vtx *v =
                            (const Vtx *)portRelocResolvePointerDebug(w1, "ftport-meshdump-vtx", 0);
                        u32 k;
                        if (v != NULL && n <= 32)
                        {
                            for (k = 0; k < n; k++)
                            {
                                port_log("MESHV: j=%d x=%d y=%d z=%d\n", (int)i,
                                         (int)v[k].v.ob[0], (int)v[k].v.ob[1], (int)v[k].v.ob[2]);
                            }
                            nv_total += (int)n;
                        }
                    }
                    else if (!((op >= 0x01 && op <= 0x07) || op == 0x00 || op >= 0xD7))
                    {
                        bailed = 1;
                        break;
                    }
                    w += 2;
                }
            }
            port_log("MESHDUMP: joint=%d nverts=%d guard=%d bailed=%d\n",
                     (int)i, nv_total, guard, bailed);
        }
    }
}

/* ========================================================================= */
/*  OpenSmash pipeline: per-frame joint matrix dump (SSB64_DUMP_FRAMES=<n>)  */
/*                                                                           */
/*  Called from ftMainProcParams (priority 0 = last fighter proc each        */
/*  frame). For the first <n> tics after fighters exist, emits one FRM       */
/*  line per live joint per fighter per frame with the full world frame      */
/*  (origin + basis rows via the 4-point walker trick). The offline          */
/*  viewer (pipeline/viewer.html) scrubs these to render injected bundles   */
/*  frame-by-frame against the vanilla skeleton.                             */
/* ========================================================================= */

extern u32 sySchedulerGetTicCount(void);
extern char *getenv(const char *);
extern int atoi(const char *);

void port_dump_frame(GObj *fighter_gobj)
{
    static int limit = -2;          /* -2 unchecked, -1 disabled, else tic budget */
    static u32 start_tic = 0;
    FTStruct *fp;
    u32 tic;
    s32 i;

    if (limit == -2)
    {
        const char *e = getenv("SSB64_DUMP_FRAMES");
        limit = (e != NULL) ? atoi(e) : -1;
        if (limit > 0)
        {
            start_tic = sySchedulerGetTicCount();
        }
    }
    if (limit <= 0)
    {
        return;
    }

    tic = sySchedulerGetTicCount();
    if (tic - start_tic >= (u32)limit)
    {
        return;
    }

    fp = ftGetStruct(fighter_gobj);
    if (fp == NULL)
    {
        return;
    }

    port_log("FRMH: t=%u pl=%d fk=%d st=%d gobj=%p pkind=%d ghost=%d root_flags=0x%x\n",
             (unsigned)(tic - start_tic), (int)fp->player, (int)fp->fkind,
             (int)fp->status_id, (void *)fighter_gobj, (int)fp->pkind,
             (int)fp->is_ghost,
             (fp->joints[0] != NULL) ? (unsigned)fp->joints[0]->flags : 0xdead);

    for (i = 0; i < FTPARTS_JOINT_NUM_MAX; i++)
    {
        DObj *j = fp->joints[i];
        Vec3f o, bx, by, bz;

        if (j == NULL)
        {
            continue;
        }

        o.x = o.y = o.z = 0.0f;
        bx.x = 1.0f; bx.y = 0.0f; bx.z = 0.0f;
        by.x = 0.0f; by.y = 1.0f; by.z = 0.0f;
        bz.x = 0.0f; bz.y = 0.0f; bz.z = 1.0f;
        gmCollisionGetFighterPartsWorldPosition(j, &o);
        gmCollisionGetFighterPartsWorldPosition(j, &bx);
        gmCollisionGetFighterPartsWorldPosition(j, &by);
        gmCollisionGetFighterPartsWorldPosition(j, &bz);

        port_log("FRM: t=%u pl=%d j=%d o=(%.4f,%.4f,%.4f) x=(%.4f,%.4f,%.4f) y=(%.4f,%.4f,%.4f) z=(%.4f,%.4f,%.4f)\n",
                 (unsigned)(tic - start_tic), (int)fp->player, (int)i,
                 o.x, o.y, o.z,
                 bx.x - o.x, bx.y - o.y, bx.z - o.z,
                 by.x - o.x, by.y - o.y, by.z - o.z,
                 bz.x - o.x, bz.y - o.y, bz.z - o.z);
    }
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
#include <stddef.h>
#include <string.h>

extern void *malloc(size_t);
extern void free(void *);
extern char *getenv(const char *);
extern int atoi(const char *);

typedef struct OSBVert { s16 x, y, z; u8 r, g, b, pad; } OSBVert;
typedef struct OSB3Vert { s16 x, y, z, s, t; u8 shade, pad; } OSB3Vert;
typedef struct OSB4Vert { s16 x, y, z, s, t; s8 nx, ny, nz; u8 pad; } OSB4Vert;

/* ---- OSB3: textured parts. One shared RGBA16 (big-endian byte pairs)
 * atlas uploaded via LoadTile (LoadBlock's 12-bit texel count tops out at
 * 64x64; LoadTile's S10.2 coords reach 1024x1024). Combiner multiplies
 * TEXEL0 * SHADE, with the baked diffuse in the vertex colors. */
static Gfx *osbBuildPartDL3(FILE *f, u32 nbatches, u8 *tex, u32 tw, u32 th, int lit)
{
    Gfx *dl;
    Gfx *g;
    Vtx *vtx_all;
    u32 total_v = 0, total_t = 0, b, voff;
    u32 vsize = lit ? sizeof(OSB4Vert) : sizeof(OSB3Vert);
    long part_start = ftell(f);

    for (b = 0; b < nbatches; b++)
    {
        u32 hdr[2];
        if (fread(hdr, 4, 2, f) != 2) return NULL;
        total_v += hdr[0];
        total_t += hdr[1];
        fseek(f, (long)(hdr[0] * vsize + hdr[1] * 4), SEEK_CUR);
    }
    fseek(f, part_start, SEEK_SET);

    vtx_all = (Vtx *)malloc(sizeof(Vtx) * total_v);
    dl = (Gfx *)malloc(sizeof(Gfx) * (32 + nbatches + total_t + 6));
    if (vtx_all == NULL || dl == NULL)
    {
        return NULL;
    }
    g = dl;

    gDPPipeSync(g++);
    if (!lit)
    {
        gDPSetCycleType(g++, G_CYC_1CYCLE);
        gDPSetRenderMode(g++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    }
    if (lit)
    {
        /* OSB4: real N64 lighting with our OWN neutral lights. The vanilla
         * material pipeline programs LIGHT_1 with the part's MATERIAL
         * color (untextured vanilla parts are colored by their lights);
         * inheriting that state multiplies our already-colored texels by
         * the vanilla part tint (~color squared) — the whole model read
         * dark and over-saturated. White key + gray ambient keeps the
         * texture as the single source of color. */
        static Lights1 sOsbLights = gdSPDefLights1(
            145, 145, 145,        /* ambient */
            255, 255, 255,        /* diffuse white */
            45, 95, 70);          /* key from front-top */
        gSPClearGeometryMode(g++, G_TEXTURE_GEN | G_CULL_BOTH);
        gSPSetGeometryMode(g++, G_LIGHTING | G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
        gSPSetLights1(g++, sOsbLights);
        /* Inherit the fighter pipeline's 2-CYCLE + G_RM_FOG_PRIM_A render
         * state (ftDisplayMain sets it right before the joint DLs, with
         * the stage-light / colanim-flash color loaded as the fog/env
         * color). Cycle 1 modulates our texture; cycle 2 passes through
         * so the blender applies the fog wash exactly like vanilla
         * fighter parts. Overriding cycle/render mode here was what made
         * the injected fighter ignore stage tint and hit flashes. */
        gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_PASS2);
    }
    else
    {
        gSPClearGeometryMode(g++, G_LIGHTING | G_TEXTURE_GEN | G_CULL_BOTH);
        gSPSetGeometryMode(g++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
        gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    }
    gDPSetTexturePersp(g++, G_TP_PERSP);
    gDPSetTextureFilter(g++, G_TF_BILERP);
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    /* upload: SETTIMG -> load tile descriptor -> LOADTILE -> render tile */
    gDPSetTextureImage(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, tw, tex);
    gDPSetTile(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (tw * 2) / 8, 0,
               G_TX_LOADTILE, 0,
               G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
    gDPLoadSync(g++);
    gDPLoadTile(g++, G_TX_LOADTILE, 0, 0, (tw - 1) << 2, (th - 1) << 2);
    gDPPipeSync(g++);
    gDPSetTile(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (tw * 2) / 8, 0,
               G_TX_RENDERTILE, 0,
               G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
    gDPSetTileSize(g++, G_TX_RENDERTILE, 0, 0, (tw - 1) << 2, (th - 1) << 2);

    voff = 0;
    for (b = 0; b < nbatches; b++)
    {
        u32 hdr[2];
        u32 i;
        OSB3Vert vraw3[30];
        OSB4Vert vraw4[30];
        u8 traw[4 * 512];

        fread(hdr, 4, 2, f);
        if (lit) fread(vraw4, sizeof(OSB4Vert), hdr[0], f);
        else     fread(vraw3, sizeof(OSB3Vert), hdr[0], f);
        fread(traw, 4, hdr[1], f);

        for (i = 0; i < hdr[0]; i++)
        {
            Vtx *v = &vtx_all[voff + i];
            if (lit)
            {
                v->n.ob[0] = vraw4[i].x;
                v->n.ob[1] = vraw4[i].y;
                v->n.ob[2] = vraw4[i].z;
                v->n.flag = 0;
                v->n.tc[0] = vraw4[i].s;
                v->n.tc[1] = vraw4[i].t;
                v->n.n[0] = vraw4[i].nx;
                v->n.n[1] = vraw4[i].ny;
                v->n.n[2] = vraw4[i].nz;
                v->n.a = 0xFF;
            }
            else
            {
                v->v.ob[0] = vraw3[i].x;
                v->v.ob[1] = vraw3[i].y;
                v->v.ob[2] = vraw3[i].z;
                v->v.flag = 0;
                v->v.tc[0] = vraw3[i].s;
                v->v.tc[1] = vraw3[i].t;
                v->v.cn[0] = vraw3[i].shade;
                v->v.cn[1] = vraw3[i].shade;
                v->v.cn[2] = vraw3[i].shade;
                v->v.cn[3] = 0xFF;
            }
        }
        gSPVertex(g++, &vtx_all[voff], hdr[0], 0);
        for (i = 0; i < hdr[1]; i++)
        {
            gSP1Triangle(g++, traw[i * 4 + 0], traw[i * 4 + 1], traw[i * 4 + 2], 0);
        }
        voff += hdr[0];
    }

    gDPPipeSync(g++);
    gSPSetGeometryMode(g++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPEndDisplayList(g++);
    return dl;
}

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
    dl = (Gfx *)malloc(sizeof(Gfx) * (8 + nbatches + total_t + 5));
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

    /* Restore conventional fighter render state — downstream DLs (the
     * drop shadow in particular) assume the combiner/geometry state the
     * original material pipeline leaves behind; leaking G_CC_SHADE turns
     * the shadow into a bright vertex-colored blob. */
    gDPPipeSync(g++);
    gSPSetGeometryMode(g++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPEndDisplayList(g++);
    return dl;
}


/* ========================================================================= */
/*  OSB5: CPU-skinned injected mesh (true smooth skinning).                  */
/*                                                                           */
/*  The whole generated mesh lives in ONE display list attached to joint 0   */
/*  (TopN). Every frame, port_osb5_skin_update() recomputes each vertex      */
/*  with full multi-bone LBS from the live joint matrices — the same         */
/*  deformation model the rigging provider previews — so joints never        */
/*  open a gap. Spawn-time inverse bind is captured at inject.               */
/* ========================================================================= */

typedef struct OSB5Vert { f32 x, y, z; s16 s, t; u8 j[4]; u8 w[4]; s8 n[3]; u8 pad; } OSB5Vert;

typedef struct OSB5State
{
    s32 njoints;
    s32 nverts;
    u32 joint_ids[32];
    /* joints whose vanilla body geometry the bundle replaces (BLNK
     * section). Superset of joint_ids when the target skeleton carries
     * geometry on joints no canonical part maps to (Yoshi's neck/hips
     * hold most of his head/body). 0 entries -> fall back to joint_ids. */
    s32 nblank;
    u32 blank_ids[64];
    /* accessory vertex pins (ACC2 section): kept-vanilla accessory roots
     * (tail, sword, ...) sit flush against the VANILLA body; the
     * replacement mesh has different proportions AND deforms as a blend
     * of joints, so each root is pinned to a MESH VERTEX, inset along the
     * vertex's inward normal. The skinner computes that vertex's world
     * position every tick anyway, so the root tracks the true surface
     * through any pose. */
    s32 naccs;
    struct { u32 joint; u32 vert; f32 embed; } accs[8];
    /* negative embed flags ORIENT-FOLLOW: the accessory's rotation is
     * slaved to the virtual chest (rd_chest * bind frame) instead of
     * replaying the vanilla anim rotation — DK's tie pointed into the
     * upright chibi body because its vanilla "hang down" was authored
     * against DK's horizontal chest. Bind frames captured at inject. */
    f32 acc_bind_m[8][3][3];
    s8 acc_bind_have;
    f32 acc_pitch[8];       /* ACC3: local pitch offset per pin */
    f32 acc_orient[8];      /* ACC3: 1.0 = orient-follow the chest */
    f32 acc_scale[8];       /* ACC3: DL scale for the pinned joint (0 = 1) */
    OSB5Vert *src;          /* source verts, spawn-world space */
    f32 (*bind_local)[4][3];/* per vert, per influence: joint-local coords */
    f32 (*bind_nrm)[4][3];  /* per vert, per influence: joint-local normal */
    Vtx *vtx;               /* live Vtx array the DL renders */
    /* Root-joint mesh DL, attached to fp->joints[0]->dl by the first skin
     * update whose joint frames validate — NOT at load time. A freshly
     * (re-)made fighter's cached part matrices (gmCollision transforms)
     * are zeroed/stale until its first anim pass, and o->vtx starts as
     * all-zeros; attaching immediately drew one frame of screen-filling
     * garbage triangles from the origin — the CSS flash on custom tiles
     * (the VS CSS re-makes the preview GObj every ~10 ticks, so it
     * flashed continuously while a token sat on a custom character). */
    Gfx *mesh_dl;
    /* Variant fit scale from the bundle's SCAL section (1.0 = none): the
     * conform keeps the chibi silhouette, which on tall small-headed
     * skeletons (samus) tops out ~20-30% above the vanilla fighter. The
     * pipeline emits the measured ratio and the game scales the fighter's
     * ROOT joint by it — the same mechanism the CSS card scales use, so
     * pose, hitboxes and feet-on-ground all stay consistent. Applied
     * idempotently in skin_update: whenever someone else rewrites the
     * root scale (CSS card scale, results screen), we detect the change
     * and re-multiply. */
    f32 fit_scale;
    f32 scl_applied;
    /* Successful skin fills since attach. The DL is attached at the SECOND
     * fill, not the first: the attach-time fill runs inside
     * ftManagerMakeFighter while the fighter still sits at its default
     * rest TRS (root at the world origin, unrotated — verified via
     * SSB64_OSB5_DEBUG); the CSS positions and poses it later that same
     * tick, so a mesh attached on fill #1 draws once at the wrong spot in
     * the rest pose. Waiting one fighter tick costs a single frame where
     * the (already-blanked) preview is simply absent — invisible in
     * practice, unlike the misplaced flash. */
    s32 fills;
    s32 dbg_ticks;          /* SSB64_OSB5_DEBUG frame-dump counter */
    /* CAN1 canonical retarget: the bundle's mesh/weights/BIND are the
     * validated MARIO build; each frame we rebuild VIRTUAL joint frames
     * (mario bone offsets driven by the target joints' rotation deltas
     * from their own spawn bind) and skin against those. */
    u8 canonical;
    /* TBND: the target's battle-spawn bind baked by the converter. When
     * present, the inject-time capture uses these instead of sampling
     * live joints — the CSS/results screens re-make fighters mid-pose
     * and live capture there poisoned every rotation delta. */
    u8 have_tbnd;
    f32 tb_slot_m[32][3][3];
    f32 tb_top_o[3], tb_top_m[3][3];
    f32 tb_cp_o[3], tb_chest_o[3];
    f32 tb_acc_m[8][3][3];
    s8 can_parent[32];      /* slot index of canonical parent, -1 = root */
    f32 can_root[3];        /* canonical anchor (ground under the chest) */
    f32 cbind_o[32][3];     /* canonical (mario) bind frames */
    f32 cbind_m[32][3][3];
    f32 tbind_inv[32][3][3];/* target joints' spawn-bind rotation inverses */
    /* CPM1: battle-bind basis of the target chest's parent. Mario's chest
     * and hips are sibling branches under this interior body joint; menu
     * figatrees animate its facing/lean even though TopN stays fixed. */
    f32 tb_cp_m[3][3];
    u8 have_tb_cp_m;
    f32 tbind0_inv[3][3];   /* target root (TopN) spawn-bind inverse */
    f32 t0m_attach[3][3];   /* TopN rotation at attach: the plain-DL
                             * display path never rebuilds the root's
                             * rotation, so the DL renders under this
                             * frozen frame — localization must match */
    s8 t0m_attach_have;
    f32 cint_bind[3];       /* chest parent's world offset from TopN at
                             * spawn bind, in TopN-bind frame. The interior
                             * chain (TransN/XRotN/YRotN) carries TRANSLATE
                             * channels some figatrees animate (the appear
                             * beams the body in from z=-323; crouches drop
                             * it) — the vanilla mesh follows them, and the
                             * virtual root must follow the deviation too */
    f32 can_scale;          /* canonical/target height ratio for accessories */
    f32 can_chainoff[32][3];/* per mapped slot: world-space bind sum of the
                             * intermediate (unmapped) nub translates between
                             * this joint and its canonical parent — their
                             * ghost matrices stack this offset onto the
                             * composition, so the exact virtual-local must
                             * subtract ts * chainoff */
    f32 can_snap[64][3];    /* unmapped joints' spawn translates (mount offsets) */
    s8 can_snap_have[64];
    s8 can_interior[64];    /* unmapped joint with mapped DESCENDANTS: its
                             * translation is absorbed by the mapped child's
                             * exact virtual-local -> emit ZERO translation */
    GObj *owner;
    /* GObjs are pool-allocated: after the owner despawns (match end, CSS
     * chip move) the next fighter can reuse the same address, and a bare
     * pointer match would blank a VANILLA fighter's joints (the broken
     * CSS previews). Ownership therefore also requires the fighter kind
     * to match; every spawn of the injected kind re-attaches and
     * refreshes both. */
    s32 owner_fkind;
    /* Which registry character this slot is wearing (-1 = vanilla /
     * legacy single-target inject). Two roster characters that declare
     * the same BASE fighter spawn as the same fkind, so owner_fkind
     * alone cannot tell them apart — the CSS reuse gate needs this to
     * know a preview is showing the wrong character. */
    s32 owner_char;
    /* Engine-authored draw state saved when we overwrite it (blank /
     * mesh attach), per joint id. dl/dls/dv share one union in DObj, so
     * a single void* captures either draw type. Lets a slot re-claim
     * that happens while the owner still LIVES (the 1P intro spawns
     * every lineup fighter with player 0, so a second injected fighter
     * evicts the first mid-screen) restore the previous owner to its
     * vanilla mesh instead of leaving it blanked + HIDDEN forever. */
    void *saved_dv[FTPARTS_JOINT_NUM_MAX];
    u8 saved_dv_valid[FTPARTS_JOINT_NUM_MAX];
    u8 saved_root_nib;          /* root FTParts flags&0xF before we forced plain-DL */
    u8 saved_root_nib_valid;
} OSB5State;

/* One mesh slot per PLAYER (0..3): a match fields at most four fighters,
 * and keying by player (not fkind) lets several injected characters share
 * the same base fighter — inevitable once the roster outgrows the twelve
 * vanilla skeletons. OSB5_SLOTS stays 12 for the fkind-indexed tables
 * (tiles, inject sets). */
#define OSB5_SLOTS 12
#define OSB5_PLAYER_SLOTS 4
static OSB5State sOsb5Slots[OSB5_PLAYER_SLOTS];
static Gfx sOsb5NullDL[2];
/* Null for parts whose flags&0xF==1: those parts' union field is a
 * Gfx** token-pair array (two-list draw), NOT a plain DL. Writing a
 * plain Gfx* there gets our first command word (gDPPipeSync,
 * 0xE7000000) dereferenced as a DL pointer — the Yoshi crash. */
static u32 sOsb5NullDLPair[2] = { 0, 0 };

static OSB5State *osb5_slot(s32 player)
{
    return ((u32)player < OSB5_PLAYER_SLOTS) ? &sOsb5Slots[player] : NULL;
}

/* Registry index resolved by the in-flight port_inject_bundle() call, read
 * by osb5_load() when it claims the slot (-1 = vanilla / legacy inject). */
static s32 sInjectCharIdx = -1;

/* -------------------------------------------------------------------- */
/* Injection roster: which bundle/ui/voice serves each fkind.            */
/* SSB64_INJECT_SET      = "path:fkind,path:fkind,..."                   */
/* SSB64_INJECT_UI_SET   = "path:fkind,..."                              */
/* SSB64_INJECT_VOICE_SET= "path:fkind,..."                              */
/* The single-target vars (SSB64_INJECT_BUNDLE/FKIND/UI/VOICE) remain    */
/* as compat and fill the target fkind's entry when the sets don't.      */
/* -------------------------------------------------------------------- */
typedef struct
{
    char bundle[512];
    char ui[512];
    char voice[512];
    s32 has_bundle;   /* 1 = from set, 2 = from single-target compat vars */
    s32 has_ui;
    s32 has_voice;
} OSB5SetEntry;
static OSB5SetEntry sInjectSet[OSB5_SLOTS];
static s32 sInjectSetParsed = 0;

s32 port_ui_target_fkind(void);

static void port_inject_parse_one_set(const char *env, size_t field_off, size_t flag_off)
{
    const char *p = getenv(env);
    while (p != NULL && *p != '\0')
    {
        const char *comma = strchr(p, ',');
        const char *end = (comma != NULL) ? comma : p + strlen(p);
        const char *colon = NULL;
        {
            const char *q;
            for (q = p; q < end; q++) if (*q == ':') colon = q;
        }
        if (colon != NULL && colon > p)
        {
            s32 fk = atoi(colon + 1);
            size_t n = (size_t)(colon - p);
            if ((u32)fk < OSB5_SLOTS && n < 511)
            {
                OSB5SetEntry *e = &sInjectSet[fk];
                char *dst = (char *)e + field_off;
                memcpy(dst, p, n);
                dst[n] = '\0';
                *(s32 *)((char *)e + flag_off) = 1;
            }
        }
        p = (comma != NULL) ? comma + 1 : NULL;
    }
}

static void port_inject_parse_set(void)
{
    if (sInjectSetParsed)
    {
        return;
    }
    sInjectSetParsed = 1;
    port_inject_parse_one_set("SSB64_INJECT_SET",
                              offsetof(OSB5SetEntry, bundle), offsetof(OSB5SetEntry, has_bundle));
    port_inject_parse_one_set("SSB64_INJECT_UI_SET",
                              offsetof(OSB5SetEntry, ui), offsetof(OSB5SetEntry, has_ui));
    port_inject_parse_one_set("SSB64_INJECT_VOICE_SET",
                              offsetof(OSB5SetEntry, voice), offsetof(OSB5SetEntry, has_voice));
    {
        s32 tfk = port_ui_target_fkind();
        if ((u32)tfk < OSB5_SLOTS)
        {
            OSB5SetEntry *e = &sInjectSet[tfk];
            const char *v;
            if (!e->has_bundle && (v = getenv("SSB64_INJECT_BUNDLE")) != NULL && strlen(v) < 511)
            {
                memcpy(e->bundle, v, strlen(v) + 1);
                e->has_bundle = 2;
            }
            if (!e->has_ui && (v = getenv("SSB64_INJECT_UI")) != NULL && strlen(v) < 511)
            {
                memcpy(e->ui, v, strlen(v) + 1);
                e->has_ui = 2;
            }
            if (!e->has_voice && (v = getenv("SSB64_INJECT_VOICE")) != NULL && strlen(v) < 511)
            {
                memcpy(e->voice, v, strlen(v) + 1);
                e->has_voice = 2;
            }
        }
    }
}

/* Bundle path for a fighter kind (NULL when that kind is not injected). */
static const char *port_inject_bundle_path(s32 fkind, s32 *from_single)
{
    port_inject_parse_set();
    if ((u32)fkind >= OSB5_SLOTS || !sInjectSet[fkind].has_bundle)
    {
        return NULL;
    }
    if (from_single != NULL)
    {
        *from_single = (sInjectSet[fkind].has_bundle == 2);
    }
    return sInjectSet[fkind].bundle;
}

/* -------------------------------------------------------------------- */
/* Character registry: the scalable roster. The shell stages a line file  */
/* (SSB64_ROSTER_FILE, default /roster.txt when present):                 */
/*   slug|assigned_fkind|bundle|ui|voice|short[|base_fkind]               */
/* Entry i lives on CSS page 1 + i/12, on the tile of its assigned fkind  */
/* (the shell lays pages out and fetches the matching skeleton variant).  */
/* base_fkind decouples the PLAYED fighter from the tile: the character   */
/* keeps its tile position but spawns as base_fkind (skeleton, moveset,   */
/* mesh variant). Absent/-1 = play as the tile's fighter (legacy).        */
/* Page 0 keeps the legacy env-var bindings (vanilla + SSB64_INJECT_*).   */
/* -------------------------------------------------------------------- */
#define PORT_CHAR_MAX 2048
typedef struct
{
    char slug[64];
    char shortname[8];
    s32  fkind;          /* home tile */
    s32  base;           /* fighter actually played; -1 = tile fighter */
    char bundle[256];
    char ui[256];
    char voice[256];
} PortChar;
static PortChar sChars[PORT_CHAR_MAX];
static s32 sNChars = 0;
static s32 sRosterParsed = 0;
static s32 sTileChar[OSB5_SLOTS] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
/* -2 = unbound (current tile binding decides), -1 = explicitly vanilla,
 * >=0 = registry character index */
static s32 sPlayerChar[4] = { -2, -2, -2, -2 };
static s32 sRosterPage = 0;

static void port_roster_field(char *dst, size_t cap, const char *src, size_t n)
{
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void port_roster_parse(void)
{
    const char *path;
    FILE *f;
    char line[1200];

    if (sRosterParsed)
    {
        return;
    }
    sRosterParsed = 1;
    path = getenv("SSB64_ROSTER_FILE");
    if (path == NULL) path = "/roster.txt";
    f = fopen(path, "r");
    if (f == NULL)
    {
        return;
    }
    while (sNChars < PORT_CHAR_MAX && fgets(line, sizeof line, f) != NULL)
    {
        /* slug|fkind|bundle|ui|voice|short[|base_fkind] */
        const char *fld[7];
        size_t len[7];
        s32 nf = 0;
        const char *q = line, *start = line;
        while (nf < 7)
        {
            if (*q == '|' || *q == '\n' || *q == '\r' || *q == '\0')
            {
                fld[nf] = start;
                len[nf] = (size_t)(q - start);
                nf++;
                if (*q != '|') break;
                start = ++q;
            }
            else q++;
        }
        if (nf < 3 || len[0] == 0)
        {
            continue;
        }
        {
            PortChar *c = &sChars[sNChars];
            memset(c, 0, sizeof *c);
            port_roster_field(c->slug, sizeof c->slug, fld[0], len[0]);
            c->fkind = atoi(fld[1]);
            if ((u32)c->fkind >= OSB5_SLOTS) c->fkind = 0;
            port_roster_field(c->bundle, sizeof c->bundle, fld[2], len[2]);
            if (nf > 3) port_roster_field(c->ui, sizeof c->ui, fld[3], len[3]);
            if (nf > 4) port_roster_field(c->voice, sizeof c->voice, fld[4], len[4]);
            if (nf > 5) port_roster_field(c->shortname, sizeof c->shortname, fld[5], len[5]);
            c->base = -1;
            if (nf > 6 && len[6] > 0)
            {
                c->base = atoi(fld[6]);
                if ((u32)c->base >= OSB5_SLOTS) c->base = -1;
            }
            sNChars++;
        }
    }
    fclose(f);
    port_log("ROSTER: %d characters loaded from %s\n", (int)sNChars, path);
    {
        const char *pg = getenv("SSB64_ROSTER_PAGE");
        void port_roster_set_page(s32 page);
        if (pg != NULL)
        {
            port_roster_set_page(atoi(pg));
        }
    }
    {
        /* per-player presets: SSB64_PLAYER_CHARS="slug0,slug1,-,-" */
        const char *pc = getenv("SSB64_PLAYER_CHARS");
        s32 pl = 0;
        while (pc != NULL && *pc != '\0' && pl < 4)
        {
            const char *comma = strchr(pc, ',');
            size_t n = (comma != NULL) ? (size_t)(comma - pc) : strlen(pc);
            s32 i;
            for (i = 0; i < sNChars; i++)
            {
                size_t k;
                s32 same = (strlen(sChars[i].slug) == n);
                for (k = 0; same && k < n; k++)
                {
                    if (sChars[i].slug[k] != pc[k]) same = 0;
                }
                if (same)
                {
                    sPlayerChar[pl] = i;
                    port_log("ROSTER: player %d preset to %s\n", (int)pl, sChars[i].slug);
                    break;
                }
            }
            pl++;
            pc = (comma != NULL) ? comma + 1 : NULL;
        }
    }
}

s32 port_roster_char_count(void)
{
    port_roster_parse();
    return sNChars;
}

/* pages: 0 = vanilla/legacy bindings; 1.. = registry dozen per page */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE
#endif
s32 port_roster_page_count(void)
{
    port_roster_parse();
    return 1 + (sNChars + OSB5_SLOTS - 1) / OSB5_SLOTS;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
s32 port_roster_page(void)
{
    return sRosterPage;
}

void port_roster_set_page(s32 page)
{
    s32 fk, i, lo, hi;
    port_roster_parse();
    if (page < 0) page = port_roster_page_count() - 1;
    if (page >= port_roster_page_count()) page = 0;
    sRosterPage = page;
    for (fk = 0; fk < OSB5_SLOTS; fk++)
    {
        sTileChar[fk] = -1;
    }
    if (page > 0)
    {
        lo = (page - 1) * OSB5_SLOTS;
        hi = lo + OSB5_SLOTS;
        if (hi > sNChars) hi = sNChars;
        for (i = lo; i < hi; i++)
        {
            sTileChar[sChars[i].fkind] = i;
        }
    }
    port_log("ROSTER: page %d/%d\n", (int)sRosterPage, (int)(port_roster_page_count() - 1));
}

static PortChar *port_char_at_tile(s32 fkind)
{
    port_roster_parse();
    if ((u32)fkind < OSB5_SLOTS && sTileChar[fkind] >= 0)
    {
        return &sChars[sTileChar[fkind]];
    }
    return NULL;
}

/* The character a spawning fighter should wear: an explicit per-player
 * binding wins (recorded at CSS selection or preset via env), else the
 * character currently bound to this fkind's tile. */
static PortChar *port_char_for_player(s32 player, s32 fkind, s32 *out_idx)
{
    PortChar *c = NULL;
    s32 idx = -1;
    port_roster_parse();
    if ((u32)player < 4 && sPlayerChar[player] != -2)
    {
        /* explicit binding (CSS selection or SSB64_PLAYER_CHARS): -1 is a
         * deliberate vanilla pick and must NOT fall through to the tile */
        if (sPlayerChar[player] >= 0 && sPlayerChar[player] < sNChars)
        {
            idx = sPlayerChar[player];
            c = &sChars[idx];
        }
    }
    else if ((u32)fkind < OSB5_SLOTS && sTileChar[fkind] >= 0)
    {
        idx = sTileChar[fkind];
        c = &sChars[idx];
    }
    if (out_idx != NULL) *out_idx = idx;
    return c;
}

/* Explicit selection binding, called by the CSS when a player's token
 * lands on a tile (fkind chosen) or leaves it. Binds the CURRENT page's
 * tile character (or explicit vanilla, -1) so the pick sticks through
 * page flips and into the match. */
void port_roster_bind_player(s32 player, s32 fkind)
{
    if ((u32)player >= 4)
    {
        return;
    }
    port_roster_parse();
    if ((u32)fkind < OSB5_SLOTS && sTileChar[fkind] >= 0)
    {
        sPlayerChar[player] = sTileChar[fkind];
        port_log("ROSTER: player %d bound to %s (tile fk%d)\n",
                 (int)player, sChars[sTileChar[fkind]].slug, (int)fkind);
    }
    else
    {
        /* vanilla tile (or legacy env tile): explicit vanilla so a stale
         * roster pick can't shadow it; legacy env bindings key off the
         * fkind path and are unaffected */
        sPlayerChar[player] = -1;
    }
}

void port_roster_unbind_player(s32 player)
{
    if ((u32)player < 4)
    {
        sPlayerChar[player] = -2;
    }
}

/* Does the player's bound character match what the CURRENT page shows on
 * this tile? Drives the CSS chip visibility across page flips. */
s32 port_roster_player_matches_tile(s32 player, s32 fkind)
{
    s32 tile;
    port_roster_parse();
    if ((u32)player >= 4 || (u32)fkind >= OSB5_SLOTS)
    {
        return 1;
    }
    tile = sTileChar[fkind];
    if (sPlayerChar[player] == -2)
    {
        return 1;               /* nothing bound yet — leave the CSS alone */
    }
    if (sPlayerChar[player] < 0)
    {
        return tile < 0;        /* vanilla pick matches vanilla tile */
    }
    return sPlayerChar[player] == tile;
}

/* The fighter kind a pick on this tile actually PLAYS as: the tile
 * character's base fighter when one is declared, else the tile itself.
 * Drives the CSS preview spawn (so the mesh variant, skeleton and idle
 * animation match the base fighter, not the tile position). */
s32 port_roster_tile_spawn_fkind(s32 fkind)
{
    PortChar *c = port_char_at_tile(fkind);
    if (c != NULL && c->base >= 0)
    {
        return c->base;
    }
    return fkind;
}

/* Does player's live CSS preview GObj already wear the character this tile
 * resolves to? The menu's reuse gate compares spawn fkinds, which is not a
 * sufficient identity: two roster characters that declare the same BASE
 * fighter both spawn as that fighter's kind, so the gate saw "no change",
 * skipped the destroy+remake, and left the previous character's mesh
 * attached — the card name and emblem updated while the mesh did not, and
 * the stale pick rode into the match. Comparing the WORN character closes
 * that hole; it also covers vanilla<->custom swaps on one fkind (page 0
 * Mario vs a page-1 character sitting on the Mario tile).
 *
 * Note this reads the TILE binding, not sPlayerChar[]: the menu calls this
 * before mnPlayersVSMakeFighter re-binds the player, so the player binding
 * still holds the OUTGOING pick at gate time. */
s32 port_roster_preview_char_matches(s32 player, void *fighter_gobj, s32 tile_fkind)
{
    OSB5State *o = osb5_slot(player);
    s32 want;
    port_roster_parse();
    want = ((u32)tile_fkind < OSB5_SLOTS) ? sTileChar[tile_fkind] : -1;
    if (o == NULL)
    {
        return 1;                       /* no mesh slot: fkind gate decides */
    }
    if (o->owner != (GObj *)fighter_gobj)
    {
        /* preview is not wearing an injected mesh — reuse only if the tile
         * does not want one either */
        return want < 0;
    }
    return o->owner_char == want;
}

/* Same remap keyed by the player's recorded binding — used when the CSS
 * hands picks to the battle: a bound character with a base fighter enters
 * the match as that fighter. Unbound / vanilla picks pass through. */
s32 port_roster_spawn_fkind(s32 player, s32 fkind)
{
    port_roster_parse();
    if ((u32)player < 4 && sPlayerChar[player] >= 0 && sPlayerChar[player] < sNChars)
    {
        PortChar *c = &sChars[sPlayerChar[player]];
        if (c->base >= 0)
        {
            return c->base;
        }
    }
    return fkind;
}

/* Winner name for the results screen ("<NAME> WINS!"): the bound roster
 * character's shortname, filtered to the letters the announce sprite set
 * covers (A-Z). NULL for vanilla/unbound players. */
const char *port_roster_player_shortname(s32 player)
{
    static char buf[9];
    port_roster_parse();
    if ((u32)player < 4 && sPlayerChar[player] >= 0 && sPlayerChar[player] < sNChars)
    {
        PortChar *c = &sChars[sPlayerChar[player]];
        const char *src = (c->shortname[0] != '\0') ? c->shortname : c->slug;
        s32 i, n = 0;
        for (i = 0; src[i] != '\0' && n < 8; i++)
        {
            char ch = src[i];
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
            if (ch >= 'A' && ch <= 'Z') buf[n++] = ch;
        }
        buf[n] = '\0';
        if (n > 0) return buf;
    }
    return NULL;
}

/* JS bridge: the shell's search dialog requests a page switch without a
 * reload; the CSS polls and applies it on its own frame. */
static volatile s32 sPageRequest = -1;
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE
#endif
void port_roster_request_page(s32 page)
{
    sPageRequest = page;
}

s32 port_roster_take_page_request(void)
{
    s32 r = sPageRequest;
    sPageRequest = -1;
    return r;
}

/* UI pack for a spawning fighter: the player's bound character wins,
 * else the tile binding / legacy env. */
static const char *port_ui_path_for_player(s32 player, s32 fkind)
{
    PortChar *c = port_char_for_player(player, fkind, NULL);
    const char *port_ui_path_for_fkind(s32 fkind);
    if (c != NULL)
    {
        return (c->ui[0] != '\0') ? c->ui : NULL;
    }
    return port_ui_path_for_fkind(fkind);
}

/* OSBV/OSBU path for a fighter kind (NULL when none). */
const char *port_ui_path_for_fkind(s32 fkind)
{
    PortChar *c = port_char_at_tile(fkind);
    if (c != NULL)
    {
        return (c->ui[0] != '\0') ? c->ui : NULL;
    }
    port_inject_parse_set();
    return ((u32)fkind < OSB5_SLOTS && sInjectSet[fkind].has_ui) ? sInjectSet[fkind].ui : NULL;
}

/* Announcer clip path for a fighter kind (NULL when none). */
const char *port_voice_path_for_fkind(s32 fkind)
{
    PortChar *c = port_char_at_tile(fkind);
    if (c != NULL)
    {
        return (c->voice[0] != '\0') ? c->voice : NULL;
    }
    port_inject_parse_set();
    return ((u32)fkind < OSB5_SLOTS && sInjectSet[fkind].has_voice) ? sInjectSet[fkind].voice : NULL;
}

/* the set of joints to blank: explicit BLNK list when present, else the
 * skinned joint set. */
static s32 osb5_blank_count(OSB5State *o)
{
    return (o->nblank > 0) ? o->nblank : o->njoints;
}

static s32 osb5_blank_id(OSB5State *o, s32 k)
{
    return (o->nblank > 0) ? (s32)o->blank_ids[k] : (s32)o->joint_ids[k];
}

static void osb5_blank_joint(OSB5State *o, FTStruct *fp, s32 jid)
{
    DObj *j = fp->joints[jid];
    FTParts *parts;
    if (j == NULL)
    {
        return;
    }
    /* save the engine's draw pointer the first time we cover it (and
     * again after any modelpart rewrite — the value is only fresh when
     * it isn't one of our sentinels) so slot eviction can restore it */
    if (o != NULL && (u32)jid < FTPARTS_JOINT_NUM_MAX &&
        j->dv != (void *)sOsb5NullDL && j->dv != (void *)sOsb5NullDLPair)
    {
        o->saved_dv[jid] = j->dv;
        o->saved_dv_valid[jid] = 1;
    }
    parts = (FTParts *)j->user_data.p;
    if (parts != NULL && (parts->flags & 0xF) == 1)
    {
        j->dls = (Gfx **)sOsb5NullDLPair;
    }
    else
    {
        j->dl = sOsb5NullDL;
    }
}

static s32 osb5_joint_is_blanked(FTStruct *fp, s32 jid)
{
    DObj *j = fp->joints[jid];
    FTParts *parts;
    if (j == NULL)
    {
        return 1;
    }
    /* the blank must match the part's CURRENT draw type — modelpart
     * swaps copy new flags onto the part, so a plain-DL blank on a part
     * that just became two-list (flags&0xF==1) reads our null DL's
     * bytes as a token array. Re-blank whenever the type flipped. */
    parts = (FTParts *)j->user_data.p;
    if (parts != NULL && (parts->flags & 0xF) == 1)
    {
        return j->dls == (Gfx **)sOsb5NullDLPair;
    }
    return j->dl == sOsb5NullDL;
}

/* TRUE if this joint's body part is replaced by the attached skinned mesh
 * (modelpart swaps must not resurrect the vanilla part over it). */
s32 port_osb5_joint_replaced(void *fighter_gobj, s32 joint_id)
{
    s32 k;
    FTStruct *fp;
    OSB5State *o;
    if (joint_id == 0)
    {
        return 0;
    }
    fp = ftGetStruct((GObj *)fighter_gobj);
    if (fp == NULL)
    {
        return 0;
    }
    o = osb5_slot((s32)fp->player);
    if (o == NULL || o->vtx == NULL || o->owner != fighter_gobj ||
        (s32)fp->fkind != o->owner_fkind)
    {
        return 0;
    }
    for (k = 0; k < osb5_blank_count(o); k++)
    {
        if (osb5_blank_id(o, k) == joint_id)
        {
            return 1;
        }
    }
    return 0;
}

/* Re-blank a replaced joint right after the engine re-points its part DL.
 * The skin-update self-heal in port_osb5_skin_update covers these writes
 * during play, but it only runs from ftMainProcParams — any window where
 * fighter processes are suspended (the pause menu suspends all of them) has
 * no tick to heal on. Blanking at the write site is tick-independent.
 * Call it AFTER parts->flags is assigned so the plain-DL / two-list choice
 * matches the part's new draw type. */
void port_osb5_reblank_joint(void *fighter_gobj, s32 joint_id)
{
    FTStruct *fp;
    if (getenv("SSB64_NO_MPGUARD") != NULL)
    {
        return;
    }
    if (!port_osb5_joint_replaced(fighter_gobj, joint_id))
    {
        return;
    }
    fp = ftGetStruct((GObj *)fighter_gobj);
    if (fp == NULL || (u32)joint_id >= FTPARTS_JOINT_NUM_MAX)
    {
        return;
    }
    osb5_blank_joint(osb5_slot((s32)fp->player), fp, joint_id);
}

/* SSB64_POSE_CAPTURE: mesh-eval capture mode. The display walk asks about
 * every GObj it would draw; answer TRUE (skip) for everything except
 * player 1's fighter, leaving one character on a clean black frame with no
 * stage, HUD, effects, or opponent. */
s32 port_pose_capture_active(void)
{
    static s32 sMode = -1;
    if (sMode < 0)
    {
        sMode = getenv("SSB64_POSE_CAPTURE") != NULL;
    }
    return sMode;
}

s32 port_pose_capture_filter(GObj *gobj)
{
    FTStruct *fp;
    if (!port_pose_capture_active())
    {
        return 0;
    }
    if (gobj->proc_display != ftDisplayMainProcDisplay)
    {
        return 1;
    }
    fp = ftGetStruct(gobj);
    return fp == NULL || fp->player != 0;
}

Gfx *port_osb5_null_dl(void)
{
    return sOsb5NullDL;
}

static void osb5_dobj_frame(DObj *j, f32 o[3], f32 m[3][3]);

/* A usable joint frame has a non-degenerate basis. Zeroed matrices (pool
 * fresh) and near-singular stale ones fail the determinant test; any real
 * pose (rotation, possibly scaled) passes with |det| ~ scale^3. */
static s32 osb5_frame_valid(f32 m[3][3])
{
    f32 det = m[0][0] * (m[1][1]*m[2][2] - m[1][2]*m[2][1])
            - m[0][1] * (m[1][0]*m[2][2] - m[1][2]*m[2][0])
            + m[0][2] * (m[1][0]*m[2][1] - m[1][1]*m[2][0]);
    if (det < 0.0f) det = -det;
    return det > 1e-4f && det < 1e6f;
}

static void osb5_joint_frame(FTStruct *fp, s32 joint, f32 o[3], f32 m[3][3])
{
    osb5_dobj_frame(fp->joints[joint], o, m);
}

static void osb5_dobj_frame(DObj *j, f32 o[3], f32 m[3][3])
{
    Vec3f vo, vx, vy, vz;
    vo.x = vo.y = vo.z = 0.0f;
    vx.x = 1.0f; vx.y = 0.0f; vx.z = 0.0f;
    vy.x = 0.0f; vy.y = 1.0f; vy.z = 0.0f;
    vz.x = 0.0f; vz.y = 0.0f; vz.z = 1.0f;
    gmCollisionGetFighterPartsWorldPosition(j, &vo);
    gmCollisionGetFighterPartsWorldPosition(j, &vx);
    gmCollisionGetFighterPartsWorldPosition(j, &vy);
    gmCollisionGetFighterPartsWorldPosition(j, &vz);
    o[0] = vo.x; o[1] = vo.y; o[2] = vo.z;
    /* columns = transformed basis vectors (local -> world) */
    m[0][0] = vx.x - vo.x; m[1][0] = vx.y - vo.y; m[2][0] = vx.z - vo.z;
    m[0][1] = vy.x - vo.x; m[1][1] = vy.y - vo.y; m[2][1] = vy.z - vo.z;
    m[0][2] = vz.x - vo.x; m[1][2] = vz.y - vo.y; m[2][2] = vz.z - vo.z;
}

static void osb5_mul3(f32 out[3][3], f32 a[3][3], f32 b[3][3])
{
    s32 r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            out[r][c] = a[r][0]*b[0][c] + a[r][1]*b[1][c] + a[r][2]*b[2][c];
}

static void osb5_inv3(f32 m[3][3], f32 out[3][3])
{
    f32 a = m[0][0], b = m[0][1], c = m[0][2];
    f32 d = m[1][0], e = m[1][1], f = m[1][2];
    f32 g = m[2][0], h = m[2][1], i = m[2][2];
    f32 A = e*i - f*h, B = -(d*i - f*g), C = d*h - e*g;
    f32 det = a*A + b*B + c*C;
    if (det > -1e-9f && det < 1e-9f) det = 1e-9f;
    out[0][0] = A/det;            out[0][1] = -(b*i - c*h)/det;  out[0][2] = (b*f - c*e)/det;
    out[1][0] = B/det;            out[1][1] = (a*i - c*g)/det;   out[1][2] = -(a*f - c*d)/det;
    out[2][0] = C/det;            out[2][1] = -(a*h - b*g)/det;  out[2][2] = (a*e - b*d)/det;
}

/* Smallest rigid rotation that aims one world-space direction at another.
 * Menu figatrees animate only a subset of the fighter hierarchy, so the
 * canonical arm frames occasionally need their direction recovered from
 * the live target joint positions. */
static void osb5_align3(f32 from[3], f32 to[3], f32 out[3][3])
{
    f32 a[3], b[3], v[3], la, lb, c, s2, k;
    s32 r, q;
    la = sqrtf(from[0]*from[0] + from[1]*from[1] + from[2]*from[2]);
    lb = sqrtf(to[0]*to[0] + to[1]*to[1] + to[2]*to[2]);
    for (r = 0; r < 3; r++) for (q = 0; q < 3; q++) out[r][q] = (r == q) ? 1.0f : 0.0f;
    if (la < 1e-6f || lb < 1e-6f) return;
    for (r = 0; r < 3; r++) { a[r] = from[r] / la; b[r] = to[r] / lb; }
    c = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    v[0] = a[1]*b[2] - a[2]*b[1];
    v[1] = a[2]*b[0] - a[0]*b[2];
    v[2] = a[0]*b[1] - a[1]*b[0];
    s2 = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    if (s2 < 1e-8f)
    {
        if (c > 0.0f) return;
        if (a[0] < 0.8f && a[0] > -0.8f)
        { v[0] = 0.0f; v[1] = -a[2]; v[2] = a[1]; }
        else
        { v[0] = -a[1]; v[1] = a[0]; v[2] = 0.0f; }
        la = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        for (r = 0; r < 3; r++) v[r] /= la;
        for (r = 0; r < 3; r++) for (q = 0; q < 3; q++)
            out[r][q] = 2.0f*v[r]*v[q] - ((r == q) ? 1.0f : 0.0f);
        return;
    }
    k = (1.0f - c) / s2;
    out[0][0] = 1.0f - k*(v[1]*v[1] + v[2]*v[2]);
    out[0][1] = -v[2] + k*v[0]*v[1];
    out[0][2] =  v[1] + k*v[0]*v[2];
    out[1][0] =  v[2] + k*v[1]*v[0];
    out[1][1] = 1.0f - k*(v[0]*v[0] + v[2]*v[2]);
    out[1][2] = -v[0] + k*v[1]*v[2];
    out[2][0] = -v[1] + k*v[2]*v[0];
    out[2][1] =  v[0] + k*v[2]*v[1];
    out[2][2] = 1.0f - k*(v[0]*v[0] + v[1]*v[1]);
}

/* SSB64_POSE_OVERRIDE=<skel file>: freeze the skinned mesh in the pose
 * given by SKELDUMP2 lines (world frames; x/y/z rows are the axis
 * images, row-vector convention) instead of the live animation —
 * apples-to-apples eval renders against the offline T-pose preview.
 * A joint=0 line overrides the root frame the mesh is rebased against,
 * so the pose lands at the live root's position/facing. Eval-only. */
#define POSE_OVR_MAX 64
#define POSE_OVR_SECS 8
typedef struct
{
    s32 from_frame;               /* active from this VI frame on */
    u8 have[POSE_OVR_MAX];
    f32 o[POSE_OVR_MAX][3];
    f32 m[POSE_OVR_MAX][3][3];    /* game convention: basis as columns */
} PoseOvrSec;
static struct
{
    s32 state;                    /* 0=unchecked, 1=active, -1=off */
    s32 nsec;
    PoseOvrSec sec[POSE_OVR_SECS];
} sPoseOvr;

static s32 pose_override_active(void)
{
    const char *path;
    FILE *f;
    char line[512];
    PoseOvrSec *cur;
    if (sPoseOvr.state != 0)
    {
        return sPoseOvr.state > 0;
    }
    sPoseOvr.state = -1;
    path = getenv("SSB64_POSE_OVERRIDE");
    if (path == NULL)
    {
        return 0;
    }
    f = fopen(path, "r");
    if (f == NULL)
    {
        port_log("POSE_OVERRIDE: cannot open %s\n", path);
        return 0;
    }
    sPoseOvr.nsec = 1;
    sPoseOvr.sec[0].from_frame = 0;
    cur = &sPoseOvr.sec[0];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        s32 j;
        f32 o0, o1, o2, x0, x1, x2, y0, y1, y2, z0, z1, z2;
        /* "POSEAT frame=N" starts a new section active from VI frame N —
         * several poses per boot, one screenshot each */
        if (sscanf(line, "POSEAT frame=%d", &j) == 1)
        {
            if (sPoseOvr.nsec < POSE_OVR_SECS)
            {
                cur = &sPoseOvr.sec[sPoseOvr.nsec++];
                memset(cur, 0, sizeof(*cur));
                cur->from_frame = j;
            }
            continue;
        }
        if (sscanf(line,
                   "SKELDUMP2: joint=%d o=(%f,%f,%f) x=(%f,%f,%f) "
                   "y=(%f,%f,%f) z=(%f,%f,%f)",
                   &j, &o0, &o1, &o2, &x0, &x1, &x2,
                   &y0, &y1, &y2, &z0, &z1, &z2) != 13)
        {
            continue;
        }
        if (j < 0 || j >= POSE_OVR_MAX)
        {
            continue;
        }
        cur->have[j] = 1;
        cur->o[j][0] = o0; cur->o[j][1] = o1; cur->o[j][2] = o2;
        /* rows are axis images -> game matrix wants them as columns */
        cur->m[j][0][0] = x0; cur->m[j][1][0] = x1; cur->m[j][2][0] = x2;
        cur->m[j][0][1] = y0; cur->m[j][1][1] = y1; cur->m[j][2][1] = y2;
        cur->m[j][0][2] = z0; cur->m[j][1][2] = z1; cur->m[j][2][2] = z2;
    }
    fclose(f);
    /* drop an empty leading section (file that starts with POSEAT) */
    if (sPoseOvr.nsec > 1)
    {
        s32 any = 0, k2;
        for (k2 = 0; k2 < POSE_OVR_MAX; k2++) any |= sPoseOvr.sec[0].have[k2];
        if (!any)
        {
            for (k2 = 1; k2 < sPoseOvr.nsec; k2++)
            {
                sPoseOvr.sec[k2 - 1] = sPoseOvr.sec[k2];
            }
            sPoseOvr.nsec--;
        }
    }
    sPoseOvr.state = 1;
    port_log("POSE_OVERRIDE: loaded %s (%d section(s))\n", path, sPoseOvr.nsec);
    return 1;
}

static PoseOvrSec *pose_override_sec(void)
{
    extern int port_get_frame_count(void);
    s32 fr = (s32)port_get_frame_count();
    s32 k, best = 0;
    for (k = 1; k < sPoseOvr.nsec; k++)
    {
        if (sPoseOvr.sec[k].from_frame <= fr &&
            sPoseOvr.sec[k].from_frame >= sPoseOvr.sec[best].from_frame)
        {
            best = k;
        }
    }
    return &sPoseOvr.sec[best];
}

static struct { f32 vjo[32][3]; f32 rd[32][3][3]; u8 valid; } sOsb5LateSeat[OSB5_PLAYER_SLOTS];

/* Menu live-direction aiming (root slots + leg segments) is only valid
 * when the TARGET rig stands like the canonical human: an upright biped
 * whose live joint directions are what the chibi body should follow.
 * The weird bodies (DK's hunch, Yoshi's horizontal spine) and the
 * ball-mode morphs (Kirby, Purin — and Pikachu's crouch) keep their
 * bespoke accepted look; aiming at those live directions folds the
 * human into their posture. Measured: upright rigs need <10° of root
 * correction (a card-pose lean), DK/Yoshi need ~50-60°. */
static s32 osb5_target_is_upright_biped(s32 fkind)
{
    switch (fkind)
    {
    case nFTKindMario:
    case nFTKindFox:
    case nFTKindSamus:
    case nFTKindLuigi:
    case nFTKindLink:
    case nFTKindCaptain:
    case nFTKindNess:
        return 1;
    default:
        return 0;
    }
}

static s32 osb5_canonical_slot_is_arm(OSB5State *o, s32 slot)
{
    s32 branch = slot, k;
    if (slot <= 0 || slot >= o->njoints) return 0;
    while ((s32)o->can_parent[branch] > 0) branch = (s32)o->can_parent[branch];
    if ((s32)o->can_parent[branch] != 0) return 0;
    for (k = 0; k < o->njoints; k++)
        if ((s32)o->can_parent[k] == branch) return 1;
    return 0;
}

/* kind-75 hook: for a MAPPED joint of a canonical fighter, emit the local
 * matrix that composes to EXACTLY the virtual frame under its canonical
 * parent's virtual frame: local = inv(Wp) * Wj, W = [rd*cbind | vjo].
 * Returns 1 and fills out_l (row-major local, translation in [3][0..2]),
 * or 0 when not applicable. Intermediate unmapped chain joints emit
 * near-zero scaled locals, so composing under the DObj parent is a close
 * stand-in for the canonical parent; residual is absorbed here at every
 * mapped joint, keeping ghost == virtual at all anchors gear hangs from. */
/* per-joint stored last-unlocked local (row-major float 4x4). store=1
 * saves m; store=0 loads into m, returning 0 when nothing stored yet. */
s32 port_osb5_seat_local(FTStruct *fp, DObj *dobj, f32 m[4][4], s32 store)
{
    static f32 sSeat[OSB5_PLAYER_SLOTS][64][4][4];
    static u8 sSeatValid[OSB5_PLAYER_SLOTS][64];
    s32 pl, jid, r, c;
    if (fp == NULL) return 0;
    pl = (s32)fp->player;
    if ((u32)pl >= OSB5_PLAYER_SLOTS) return 0;
    for (jid = 1; jid < FTPARTS_JOINT_NUM_MAX && jid < 64; jid++)
        if (fp->joints[jid] == dobj) break;
    if (jid >= FTPARTS_JOINT_NUM_MAX || jid >= 64) return 0;
    if (store)
    {
        for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) sSeat[pl][jid][r][c] = m[r][c];
        sSeatValid[pl][jid] = 1;
        return 1;
    }
    if (!sSeatValid[pl][jid]) return 0;
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) m[r][c] = sSeat[pl][jid][r][c];
    return 1;
}

/* is this DObj a RENDERING accessory mount of a canonical fighter (unmapped
 * joint that carries a display list)? Those are the gear joints whose
 * anim-lock snapshots need the in-hand/stow magnitude split; body chain
 * bones (no DL) must never be hidden. */
s32 port_osb5_is_gear(FTStruct *fp, DObj *dobj)
{
    OSB5State *o;
    s32 pl, kk;
    if (fp == NULL || dobj == NULL) return 0;
    pl = (s32)fp->player;
    if ((u32)pl >= OSB5_PLAYER_SLOTS) return 0;
    o = osb5_slot(pl);
    if (o == NULL || !o->canonical || o->vtx == NULL) return 0;
    if (dobj->dv == NULL || dobj->dv == (void *)sOsb5NullDL) return 0;
    for (kk = 0; kk < o->njoints; kk++)
    {
        s32 tid = (s32)o->joint_ids[kk];
        if (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX && fp->joints[tid] == dobj)
            return 0;   /* mapped body joint */
    }
    return 1;
}

s32 port_osb5_virtual_local(FTStruct *fp, DObj *dobj, f32 out_l[4][4])
{
    OSB5State *o;
    s32 pl, kk, slot = -1, pslot;
    f32 Wj[3][3], Wp[3][3], WpInv[3][3], d[3];
    s32 r, c;
    if (fp == NULL) return 0;
    pl = (s32)fp->player;
    if ((u32)pl >= OSB5_PLAYER_SLOTS) return 0;
    o = osb5_slot(pl);
    if (o == NULL || !o->canonical || o->vtx == NULL || !sOsb5LateSeat[pl].valid)
        return 0;
    for (kk = 0; kk < o->njoints; kk++)
    {
        s32 tid = (s32)o->joint_ids[kk];
        if (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX && fp->joints[tid] == dobj)
            slot = kk;   /* last slot wins (collapsed chains) */
    }
    if (slot < 0) return 0;
    pslot = (s32)o->can_parent[slot];
    /* collapsed chains (samus cannon: canonical forearm AND hand map onto
     * target joint 16) make the canonical parent resolve to the SAME
     * target joint — self-parenting degenerates the local. Walk up until
     * the target id differs. */
    while (pslot >= 0 && (s32)o->joint_ids[pslot] == (s32)o->joint_ids[slot])
        pslot = (s32)o->can_parent[pslot];
    /* world rotations: rd * cbind */
    osb5_mul3(Wj, sOsb5LateSeat[pl].rd[slot], o->cbind_m[slot]);
    if (pslot >= 0)
    {
        osb5_mul3(Wp, sOsb5LateSeat[pl].rd[pslot], o->cbind_m[pslot]);
        osb5_inv3(Wp, WpInv);
        d[0] = sOsb5LateSeat[pl].vjo[slot][0] - sOsb5LateSeat[pl].vjo[pslot][0];
        d[1] = sOsb5LateSeat[pl].vjo[slot][1] - sOsb5LateSeat[pl].vjo[pslot][1];
        d[2] = sOsb5LateSeat[pl].vjo[slot][2] - sOsb5LateSeat[pl].vjo[pslot][2];
    }
    else
    {
        /* root-anchored: parent is the fighter's TopN */
        f32 t0o[3];
        osb5_joint_frame(fp, 0, t0o, Wp);
        osb5_inv3(Wp, WpInv);
        d[0] = sOsb5LateSeat[pl].vjo[slot][0] - t0o[0];
        d[1] = sOsb5LateSeat[pl].vjo[slot][1] - t0o[1];
        d[2] = sOsb5LateSeat[pl].vjo[slot][2] - t0o[2];
    }
    /* local rotation = WpInv * Wj; local translation = WpInv * d */
    {
        f32 L[3][3];
        osb5_mul3(L, WpInv, Wj);
        for (r = 0; r < 3; r++)
        {
            /* engine Mtx44f is row-vector (rows = basis images); our
             * composition is column-convention — transpose on write */
            for (c = 0; c < 3; c++) out_l[r][c] = L[c][r];
            out_l[r][3] = 0.0f;
        }
        if (slot < 32)
        {
            /* the intermediate nubs' ghost matrices add ts*chainoff to the
             * composition before this local applies — pre-subtract it */
            d[0] -= o->can_scale * o->can_chainoff[slot][0];
            d[1] -= o->can_scale * o->can_chainoff[slot][1];
            d[2] -= o->can_scale * o->can_chainoff[slot][2];
        }
        out_l[3][0] = WpInv[0][0]*d[0] + WpInv[0][1]*d[1] + WpInv[0][2]*d[2];
        out_l[3][1] = WpInv[1][0]*d[0] + WpInv[1][1]*d[1] + WpInv[1][2]*d[2];
        out_l[3][2] = WpInv[2][0]*d[0] + WpInv[2][1]*d[1] + WpInv[2][2]*d[2];
        out_l[3][3] = 1.0f;
    }
    return 1;
}

static void osb5_reseat_kept(OSB5State *o, FTStruct *fp, f32 vjo[32][3])
{
    f32 (*rd)[3][3] = sOsb5LateSeat[fp->player].rd;
    s32 kk, jj, r;
    static s32 sLogged = 0;
    if (!sLogged) { port_log("OSB5: RESEAT ACTIVE\n"); sLogged = 1; }
    for (kk = 0; kk < o->njoints; kk++)
    {
        s32 tid = (s32)o->joint_ids[kk];
        s32 kept = 1;
        DObj *aj;
        f32 po[3], pm[3][3], pinv[3][3], d[3];
        if (tid <= 0 || tid >= FTPARTS_JOINT_NUM_MAX)
            continue;
        for (jj = kk + 1; jj < o->njoints; jj++)
            if ((s32)o->joint_ids[jj] == tid) { kept = 0; break; }
        if (!kept)
            continue;
        aj = fp->joints[tid];
        if (aj == NULL || aj->parent == NULL)
            continue;
        osb5_dobj_frame(aj->parent, po, pm);
        osb5_inv3(pm, pinv);
        d[0] = vjo[kk][0] - po[0];
        d[1] = vjo[kk][1] - po[1];
        d[2] = vjo[kk][2] - po[2];
        aj->translate.vec.f.x = pinv[0][0]*d[0] + pinv[0][1]*d[1] + pinv[0][2]*d[2];
        aj->translate.vec.f.y = pinv[1][0]*d[0] + pinv[1][1]*d[1] + pinv[1][2]*d[2];
        aj->translate.vec.f.z = pinv[2][0]*d[0] + pinv[2][1]*d[1] + pinv[2][2]*d[2];
        /* one-frame-flash fix, scoped: if THIS reseated joint is
         * anim-locked, its snapshot was captured from the pre-reseat
         * transform on the engage tick — re-capture it from the value we
         * just wrote. Only reseated joints: their TRS is always ours;
         * gear mounts keep the engine's own snapshots. */
        {
            FTParts *pt = (FTParts *)aj->user_data.p;
            if (pt != NULL && pt->transform_update_mode != 0)
            {
                extern void gmCollisionTransformMatrixAll(DObj *dobj, FTParts *parts, Mtx44f mtx);
                gmCollisionTransformMatrixAll(aj, pt, pt->unk_dobjtrans_0x10);
            }
        }
    }
    (void)r;

    /* THE one-frame-flash fix: the engine captures anim-lock snapshot
     * matrices (parts->unk_dobjtrans_0x10) when a lock engages — BEFORE
     * this reseat has moved the joints that tick — so the first locked
     * frame renders from pre-reseat values. Re-capture every locked
     * joint's snapshot from the CURRENT (post-reseat) TRS. */

    /* unmapped-joint compression: every fighter joint OUTSIDE the mapped
     * set (accessory mounts, sheathed-gear copies, intermediate chain
     * joints) keeps following the tall real skeleton's arcs. Scaling each
     * unmapped joint's LOCAL translate by the canonical/target height
     * ratio compresses those arcs toward the mapped ancestors at any
     * chain depth — the sheathe animation happens, at chibi scale. Runs
     * every tick right after the animation wrote fresh translates. */
    {
        s32 jid, kk2;
        s8 mapped[FTPARTS_JOINT_NUM_MAX];
        for (jid = 0; jid < FTPARTS_JOINT_NUM_MAX; jid++) mapped[jid] = 0;
        mapped[0] = 1;
        for (kk2 = 0; kk2 < o->njoints; kk2++)
        {
            s32 tid = (s32)o->joint_ids[kk2];
            if (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX) mapped[tid] = 1;
        }
        if (getenv("SSB64_COMPRESS_DEBUG") != NULL)
        {
            static s32 sLog = 0;
            if (sLog < 3 && fp->joints[19] != NULL && fp->joints[20] != NULL)
            {
                port_log("COMPRESS: scale=%.2f j11t=(%.1f,%.1f,%.1f) j19t=(%.1f,%.1f,%.1f) j20t=(%.1f,%.1f,%.1f)\n",
                         o->can_scale,
                         fp->joints[11] ? fp->joints[11]->translate.vec.f.x : -999.0f,
                         fp->joints[11] ? fp->joints[11]->translate.vec.f.y : -999.0f,
                         fp->joints[11] ? fp->joints[11]->translate.vec.f.z : -999.0f,
                         fp->joints[19]->translate.vec.f.x, fp->joints[19]->translate.vec.f.y, fp->joints[19]->translate.vec.f.z,
                         fp->joints[20]->translate.vec.f.x, fp->joints[20]->translate.vec.f.y, fp->joints[20]->translate.vec.f.z);
                sLog++;
            }
        }
        /* unmapped joints are handled at matrix-build time via
     * port_osb5_dobj_tscale() — translate writes here are overwritten by
     * the lazy animation evaluation during display and never render */
    }
}

static f32 sOsb5SeatWrote[OSB5_PLAYER_SLOTS][3];

void port_osb5_drop_probe(GObj *fighter_gobj, const char *site)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    OSB5State *o;
    static s32 sT = 0;
    f32 w11[3] = {0,0,0}, w19[3] = {0,0,0}, w20[3] = {0,0,0}, m_[3][3];
    if (getenv("SSB64_DROP_DEBUG") == NULL) return;
    if (fp == NULL || fp->player != 0) return;
    o = osb5_slot(0);
    if (o == NULL || !o->canonical) return;
    if (fp->joints[11]) osb5_dobj_frame(fp->joints[11], w11, m_);
    if (fp->joints[19]) osb5_dobj_frame(fp->joints[19], w19, m_);
    if (fp->joints[20]) osb5_dobj_frame(fp->joints[20], w20, m_);
    port_log("DROP2[%s %d]: j11=(%.0f,%.0f,%.0f) j19=(%.0f,%.0f,%.0f) j20=(%.0f,%.0f,%.0f) dv=%d%d%d\n",
             site, sT++,
             w11[0], w11[1], w11[2], w19[0], w19[1], w19[2], w20[0], w20[1], w20[2],
             fp->joints[11] && fp->joints[11]->dv && fp->joints[11]->dv != (void*)sOsb5NullDL,
             fp->joints[19] && fp->joints[19]->dv && fp->joints[19]->dv != (void*)sOsb5NullDL,
             fp->joints[20] && fp->joints[20]->dv && fp->joints[20]->dv != (void*)sOsb5NullDL);
}

f32 port_osb5_dobj_tscale(DObj *dobj)
{
    /* canonical retarget: local-translate scale at matrix-build time.
     * 1.0 for everything except UNMAPPED registry joints of a fighter
     * with an active canonical injection — their animation tracks move
     * gear at vanilla amplitudes on the taller skeleton. */
    GObj *g;
    FTStruct *fp;
    OSB5State *o;
    s32 pl, jid, kk;
    if (1) return 1.0f;   /* NEUTRALIZED: state-1 revert — the ghost
                            * skeleton stays at vanilla scale; gear rides
                            * reseated joints + refreshed snapshots */
    if (dobj == NULL) return 1.0f;
    g = dobj->parent_gobj;
    if (g == NULL) return 1.0f;
    for (pl = 0; pl < OSB5_PLAYER_SLOTS; pl++)
    {
        o = &sOsb5Slots[pl];
        if (o->owner != g || !o->canonical || o->vtx == NULL) continue;
        if (o->can_scale <= 0.05f || o->can_scale >= 0.98f) return 1.0f;
        fp = ftGetStruct(g);
        if (fp == NULL) return 1.0f;
        if (fp->joints[0] == dobj) return 1.0f;
        /* scale EVERY non-root joint: the DObj tree is an invisible
         * full-height vanilla skeleton that all vanilla-rendered parts
         * (gear, kept accessories) ride; compressing every local
         * translate shrinks that whole skeleton to chibi height about
         * the root. The injected mesh is CPU-skinned from the virtual
         * frames and never reads these matrices. */
        for (kk = 0; kk < o->njoints; kk++)
        {
            s32 tid = (s32)o->joint_ids[kk];
            if (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX && fp->joints[tid] == dobj)
                return 1.0f;   /* mapped: handled by the exact virtual-local */
        }
        {
            extern double atof(const char *);
            const char *fs = getenv("SSB64_TSCALE_FORCE");
            if (fs != NULL) return (f32)atof(fs);
        }
        if (getenv("SSB64_TSCALE_DEBUG") != NULL)
        {
            static s32 sN = 0;
            if (sN < 8)
            {
                port_log("TSCALE: scaling dobj %p (t=%.1f,%.1f,%.1f)\n", (void *)dobj,
                         dobj->translate.vec.f.x, dobj->translate.vec.f.y, dobj->translate.vec.f.z);
                sN++;
            }
        }
        return o->can_scale;
    }
    return 1.0f;
}

void port_osb5_dl_debug(GObj *fighter_gobj)
{
    (void)fighter_gobj;   /* superseded by port_osb5_drop_probe */
}

void port_osb5_heal_blanks(GObj *fighter_gobj)
{
    /* display-proc self-heal: face-blink / model-part / LOD code that runs
     * AFTER the params-proc heal re-points vanilla DLs onto replaced
     * joints for a single frame — the vanilla arm (with sword and shield)
     * flashes in at the tall skeleton's positions. Heal again right
     * before the DL build. */
    FTStruct *fp = ftGetStruct(fighter_gobj);
    OSB5State *o;
    s32 k;
    if (fp == NULL) return;
    o = osb5_slot((s32)fp->player);
    if (o == NULL || o->vtx == NULL || o->owner != fighter_gobj) return;
    for (k = 0; k < osb5_blank_count(o); k++)
    {
        s32 jid = osb5_blank_id(o, k);
        if (jid != 0 && jid < FTPARTS_JOINT_NUM_MAX && !osb5_joint_is_blanked(fp, jid))
        {
            osb5_blank_joint(o, fp, jid);
        }
    }
}

void port_osb5_reseat_late(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    OSB5State *o;
    if (fp == NULL) return;
    o = osb5_slot((s32)fp->player);
    if (o == NULL || !o->canonical || o->owner != fighter_gobj) return;
    if (!sOsb5LateSeat[fp->player].valid) return;

    /* accessory state-swap blink: when an unmapped joint's display list
     * pointer CHANGES (in-hand gear <-> sheathed copies), its matrix
     * track is stale for that first tick and the gear renders mid-air at
     * raw offsets. Hide the joint for exactly that one tick — a 1-frame
     * blink at 60fps beats a mid-air pop. */
    if (0)
    {
        static void *sLastDv[OSB5_PLAYER_SLOTS][64];
        static void *sSavedDv[OSB5_PLAYER_SLOTS][64];
        static u8 sPending[OSB5_PLAYER_SLOTS][64];
        s32 jid2, kk2, m2;
        s32 pl = (s32)fp->player;
        for (jid2 = 1; jid2 < FTPARTS_JOINT_NUM_MAX && jid2 < 64; jid2++)
        {
            DObj *dj = fp->joints[jid2];
            void *cur;
            if (dj == NULL) continue;
            m2 = 0;
            for (kk2 = 0; kk2 < o->njoints; kk2++)
                if ((s32)o->joint_ids[kk2] == jid2) { m2 = 1; break; }
            if (m2) continue;
            if (sPending[pl][jid2])
            {
                dj->dv = sSavedDv[pl][jid2];
                sPending[pl][jid2] = 0;
            }
            cur = dj->dv;
            {
                /* hide for exactly one tick when the anim-lock engages or
                 * releases (transform_update_mode changes): the snapshot
                 * captured on the engage tick holds a transitional pose
                 * that renders one frame of mid-air gear */
                FTParts *pt = (FTParts *)dj->user_data.p;
                s32 mode = (pt != NULL) ? (s32)pt->transform_update_mode : -1;
                {
                    static s8 sLastMode[OSB5_PLAYER_SLOTS][64];
                    if (sLastMode[pl][jid2] != (s8)mode)
                    {
                        if (cur != NULL && cur != (void *)sOsb5NullDL
                            && sLastMode[pl][jid2] != -128)
                        {
                            sSavedDv[pl][jid2] = cur;
                            dj->dv = (void *)sOsb5NullDL;
                            sPending[pl][jid2] = 1;
                        }
                        sLastMode[pl][jid2] = (s8)mode;
                    }
                }
            }
            sLastDv[pl][jid2] = cur;
        }
    }
    if (getenv("SSB64_NO_RESEAT") != NULL) return;
    osb5_reseat_kept(o, fp, sOsb5LateSeat[fp->player].vjo);
    /* invalidate the part-matrix memo AFTER the writes: the draw pass
     * consumes memoized matrices, and without this the re-seat is only
     * visible on ticks where some later engine branch happened to
     * re-invalidate — the alternating-frame accessory flicker. */
    {
        extern void ftParamsUpdateFighterPartsTransformAll(DObj *root_dobj);
        if (fp->joints[0] != NULL)
            ftParamsUpdateFighterPartsTransformAll(fp->joints[0]);
    }
    if (getenv("SSB64_SEAT_DEBUG") != NULL && o->njoints > 3)
    {
        /* probe: canonical hand slot 3 (mario joint 10) */
        s32 tid = (s32)o->joint_ids[3];
        DObj *aj = (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX) ? fp->joints[tid] : NULL;
        if (aj != NULL)
        {
            sOsb5SeatWrote[fp->player][0] = aj->translate.vec.f.x;
            sOsb5SeatWrote[fp->player][1] = aj->translate.vec.f.y;
            sOsb5SeatWrote[fp->player][2] = aj->translate.vec.f.z;
        }
    }
}

void port_osb5_seat_probe(GObj *fighter_gobj, const char *site)
{
    /* call from arbitrary points: log the hand translate vs what the late
     * reseat wrote, revealing WHO moves it and when */
    FTStruct *fp = ftGetStruct(fighter_gobj);
    OSB5State *o;
    s32 tid;
    DObj *aj;
    if (getenv("SSB64_SEAT_DEBUG") == NULL) return;
    if (fp == NULL || fp->player != 0) return;
    o = osb5_slot(0);
    if (o == NULL || !o->canonical || o->njoints <= 3) return;
    tid = (s32)o->joint_ids[3];
    aj = (tid > 0 && tid < FTPARTS_JOINT_NUM_MAX) ? fp->joints[tid] : NULL;
    if (aj == NULL) return;
    port_log("SEAT[%s]: t=(%.1f,%.1f,%.1f) wrote=(%.1f,%.1f,%.1f)\n", site,
             aj->translate.vec.f.x, aj->translate.vec.f.y, aj->translate.vec.f.z,
             sOsb5SeatWrote[0][0], sOsb5SeatWrote[0][1], sOsb5SeatWrote[0][2]);
}

void port_osb5_skin_update(GObj *fighter_gobj);

void port_osb5_proc_post(GObj *fighter_gobj)
{
    extern void port_osb5_copy_windows(void);
    port_osb5_skin_update(fighter_gobj);
    port_osb5_copy_windows();
    port_osb5_reseat_late(fighter_gobj);
}

/* Projectile/muzzle lever-arm scale for gameplay code: hardcoded joint
 * offsets (samus's 180u cannon muzzle) are sized for the vanilla body;
 * scale them down to the canonical (chibi) proportions so shots leave the
 * visible gun tip. 1.0 for vanilla and non-canonical fighters. */
f32 port_osb5_charge_scale(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    OSB5State *o;
    if (fp == NULL) return 1.0f;
    o = osb5_slot((s32)fp->player);
    if (o == NULL || o->vtx == NULL || o->owner != fighter_gobj ||
        (s32)fp->fkind != o->owner_fkind || !o->canonical)
        return 1.0f;
    return o->can_scale;
}

static void osb5_skin_update_body(GObj *fighter_gobj);

/* Menu scenes (CSS previews, results podium) animate fighters without the
 * battle tick's per-frame invalidation of transform_update_mode. The
 * kind-75 display func draws from live TRS only while that mode is 0; any
 * collision query we make flips the queried chain to snapshot mode, and
 * the display then renders the FROZEN snapshot forever — the select-card
 * banana bend, and the rigid mid-anim tumble when the snapshots were
 * force-refreshed a frame behind. So on menu scenes, clear the mode across
 * the rig before skinning (our walk reads live TRS) and again after (the
 * display walk composes fresh TRS, exactly like an uninjected preview).
 * Never in battle: there the game owns the invalidation, and anim-locked
 * joints carry garbage TRS mid-lock. */
/* joints the canonical reseat wrote this tick on a menu scene: their
 * snapshots carry the reseated translate and must stay authoritative
 * (mode 1) through the display walk, because menu anims advance lazily
 * DURING that walk and would otherwise overwrite the reseat with the
 * vanilla-skeleton translate — Link's sword/shield floated at the chibi
 * preview's face while carrying the token. */
static u8 sOsb5MenuSeated[OSB5_PLAYER_SLOTS][64];

static s32 osb5_on_menu_scene(void)
{
    extern s32 port_current_scene(void);
    s32 sc = port_current_scene();
    return (sc == 16 || sc == 17 || sc == 18 || sc == 19 || sc == 20 || sc == 24);
}

static void osb5_menu_unfreeze(GObj *fighter_gobj, s32 keep_seated)
{
    FTStruct *fp;
    s32 j, pl;
    if (!osb5_on_menu_scene())
        return;
    fp = ftGetStruct(fighter_gobj);
    if (fp == NULL) return;
    pl = (s32)fp->player;
    for (j = 0; j < FTPARTS_JOINT_NUM_MAX; j++)
    {
        DObj *dj = fp->joints[j];
        FTParts *pt = (dj != NULL) ? (FTParts *)dj->user_data.p : NULL;
        if (pt == NULL) continue;
        if (!keep_seated && (u32)pl < OSB5_PLAYER_SLOTS && j < 64)
            sOsb5MenuSeated[pl][j] = 0;
        if (keep_seated && (u32)pl < OSB5_PLAYER_SLOTS && j < 64 &&
            sOsb5MenuSeated[pl][j])
            continue;
        /* ONLY the lazy collision cache (mode 1). Mode 3 is an ANIM LOCK:
         * ftParamSetAnimLocks froze that joint's snapshot on purpose and
         * its TRS is garbage until ftParamClearAnimLocks releases it —
         * flattening 3 -> 0 mid-anim twisted the selected preview. */
        if (pt->transform_update_mode == 1)
            pt->transform_update_mode = 0;
    }
}

void port_osb5_skin_update(GObj *fighter_gobj)
{
    if (getenv("SSB64_CSS_DEBUG") != NULL)
    {
        extern s32 port_current_scene(void);
        static s32 sCssDbg = 0;
        FTStruct *fpd = ftGetStruct(fighter_gobj);
        s32 scd = port_current_scene();
        if (fpd != NULL && (s32)fpd->player == 0 &&
            (scd == 16 || scd == 24) && ((sCssDbg++ % 20) == 0))
        {
            extern float atan2f(float, float);
            s32 jd;
            s32 nm0 = 0, nm1 = 0, nm3 = 0, nmx = 0;
            for (jd = 0; jd < FTPARTS_JOINT_NUM_MAX; jd++)
            {
                DObj *dj = fpd->joints[jd];
                FTParts *pt = (dj != NULL) ? (FTParts *)dj->user_data.p : NULL;
                if (pt == NULL) continue;
                if (pt->transform_update_mode == 0) nm0++;
                else if (pt->transform_update_mode == 1) nm1++;
                else if (pt->transform_update_mode == 3) nm3++;
                else nmx++;
            }
            {
                u32 fsum = 0;
                if (fpd->figatree != NULL)
                {
                    const u32 *fw = (const u32 *)fpd->figatree;
                    s32 fi;
                    for (fi = 0; fi < 0x180; fi++) fsum += fw[fi];
                }
                port_log("CSSDBG t=%d locks=%d motion=%d modes 0/1/3/x=%d/%d/%d/%d figsum=0x%08x\n",
                         sCssDbg, (int)fpd->is_use_animlocks, (int)fpd->motion_id,
                         nm0, nm1, nm3, nmx, fsum);
            }
            for (jd = 0; jd < FTPARTS_JOINT_NUM_MAX; jd++)
            {
                DObj *dj = fpd->joints[jd];
                FTParts *pt = (dj != NULL) ? (FTParts *)dj->user_data.p : NULL;
                if (pt == NULL) continue;
                port_log("CSSDBG  j%d m%d dj=%p gobj=%p rot=(%.3f %.3f %.3f) tra=(%.1f %.1f %.1f)\n",
                         jd, (int)pt->transform_update_mode, (void *)dj, (void *)fighter_gobj,
                         dj->rotate.vec.f.x, dj->rotate.vec.f.y, dj->rotate.vec.f.z,
                         dj->translate.vec.f.x, dj->translate.vec.f.y, dj->translate.vec.f.z);
                if (jd == 20)
                {
                    AObj *ao = dj->aobj;
                    char tl[96];
                    s32 tn = 0;
                    tl[0] = '\0';
                    while (ao != NULL && tn < 80)
                    {
                        tn += snprintf(tl + tn, sizeof(tl) - tn, "%d:%d ",
                                       (int)ao->track, (int)ao->kind);
                        ao = ao->next;
                    }
                    port_log("CSSDBG  j20 aobjs [%s] wait=%.1f\n", tl, dj->anim_wait);
                }
            }
            for (jd = 0; jd <= 3; jd++)
            {
                DObj *dj = fpd->joints[jd];
                FTParts *pt = (dj != NULL) ? (FTParts *)dj->user_data.p : NULL;
                f32 co[3], cm[3][3];
                if (dj == NULL) continue;
                osb5_dobj_frame(dj, co, cm);
                port_log("CSSDBG  j%d mode=%d 0x5=%d trs_rot=(%.2f %.2f %.2f) collYaw=%.2f collPitch=%.2f o=(%.0f %.0f %.0f)\n",
                         jd,
                         pt != NULL ? (int)pt->transform_update_mode : -1,
                         pt != NULL ? (int)pt->unk_dobjtrans_0x5 : -1,
                         dj->rotate.vec.f.x, dj->rotate.vec.f.y, dj->rotate.vec.f.z,
                         atan2f(cm[2][0], cm[0][0]),
                         atan2f(-cm[1][0], cm[1][1]),
                         co[0], co[1], co[2]);
            }
        }
    }
    osb5_menu_unfreeze(fighter_gobj, 0);
    osb5_skin_update_body(fighter_gobj);
    osb5_menu_unfreeze(fighter_gobj, 1);
}

static void osb5_skin_update_body(GObj *fighter_gobj)
{
    FTStruct *fp;
    OSB5State *o;
    f32 jo[32][3], jm[32][3][3];
    f32 t0o[3], t0m[3][3], t0inv[3][3];
    s32 k, i;

    if (getenv("SSB64_NO_SKIN") != NULL) return;
    if (getenv("SSB64_FACE_DEBUG") != NULL)
    {
        static s32 sFcDbg = 0;
        FTStruct *fpd = ftGetStruct(fighter_gobj);
        if (fpd != NULL && fpd->joints[0] != NULL && sFcDbg < 3000 && ((sFcDbg++ % 30) == 0))
        {
            extern float atan2f(float, float);
            f32 co[3], cm[3][3];
            FTParts *pt0 = (FTParts *)fpd->joints[0]->user_data.p;
            osb5_dobj_frame(fpd->joints[0], co, cm);
            port_log("FACEDBG lr=%d rotY=%.2f collYaw=%.2f mode=%d\n",
                     (int)fpd->lr,
                     fpd->joints[0]->rotate.vec.f.y,
                     atan2f(cm[2][0], cm[0][0]),
                     pt0 != NULL ? (int)pt0->transform_update_mode : -1);
        }
    }
    fp = ftGetStruct(fighter_gobj);
    if (fp == NULL) return;
    o = osb5_slot((s32)fp->player);
    if (o == NULL || o->vtx == NULL || o->owner != fighter_gobj)
    {
        if (getenv("SSB64_OSB5_DEBUG") != NULL && o != NULL && o->vtx != NULL && o->dbg_ticks < 3)
        {
            port_log("OSB5DBG: bail owner: slot owner=%p gobj=%p player=%d\n",
                     (void *)o->owner, (void *)fighter_gobj, (int)fp->player);
            o->dbg_ticks++;
        }
        return;
    }
    if ((s32)fp->fkind != o->owner_fkind)
    {
        if (getenv("SSB64_OSB5_DEBUG") != NULL && o->dbg_ticks < 3)
        {
            port_log("OSB5DBG: bail fkind: fp=%d owner_fkind=%d\n",
                     (int)fp->fkind, (int)o->owner_fkind);
            o->dbg_ticks++;
        }
        return;
    }

    /* Variant fit scale: multiply into the root joint scale, rebasing
     * whenever an external writer (CSS card scale, results screen) has
     * replaced our last value. Root joint 0 (TopN) sits at ground level,
     * so the character scales about its feet. */
    if (o->fit_scale < 0.995f && fp->joints[0] != NULL)
    {
        f32 cur = fp->joints[0]->scale.vec.f.x;
        if (cur != o->scl_applied && cur > 0.0f)
        {
            o->scl_applied = cur * o->fit_scale;
            fp->joints[0]->scale.vec.f.x = o->scl_applied;
            fp->joints[0]->scale.vec.f.y = o->scl_applied;
            fp->joints[0]->scale.vec.f.z = o->scl_applied;
        }
    }

    /* Invalidate the gmCollision per-part matrix memo for the whole
     * skeleton before reading joint frames (same call the engine makes
     * after transform mutations — see ftmain.c and the character
     * specials). A freshly (re-)made fighter's pool-recycled FTParts can
     * carry stale memo flags pointing at the PREVIOUS owner's matrices;
     * skinning against those exploded the mesh into screen-filling
     * garbage for one frame — the CSS custom-tile flash (the VS CSS
     * re-makes the preview GObj every ~10 ticks, so it flashed
     * continuously). After this, the queries below recompute from the
     * live TRS state, which IS valid at attach time (figatree attach has
     * already run inside ftManagerMakeFighter). */
    {
        extern void ftParamsUpdateFighterPartsTransformAll(DObj *root_dobj);
        if (fp->joints[0] != NULL)
        {
            ftParamsUpdateFighterPartsTransformAll(fp->joints[0]);
        }
    }

    if (getenv("SSB64_NO_SELFHEAL") == NULL)
    {
    /* self-heal: modelpart/detail code has several sites that re-point a
     * part DL (face blinks, LOD switches, respawn resets). Whatever wrote
     * a vanilla DL onto a replaced joint, blank it again this tick. */
    for (k = 0; k < osb5_blank_count(o); k++)
    {
        s32 jid = osb5_blank_id(o, k);
        if (jid != 0 && jid < FTPARTS_JOINT_NUM_MAX && !osb5_joint_is_blanked(fp, jid))
        {
            osb5_blank_joint(o, fp, jid);
        }
    }
    /* keep the root on the plain-DL path (modelpart swaps copy flags) */
    if (fp->joints[0] != NULL)
    {
        FTParts *rparts = (FTParts *)fp->joints[0]->user_data.p;
        if (rparts != NULL && (rparts->flags & 0xF) != 0)
        {
            o->saved_root_nib = (u8)(rparts->flags & 0xF);
            o->saved_root_nib_valid = 1;
            rparts->flags &= ~0xF;
        }
    }
    }
    for (k = 0; k < o->njoints; k++)
    {
        s32 jid = (s32)o->joint_ids[k];
        if (fp->joints[jid] == NULL)
        {
            if (getenv("SSB64_OSB5_DEBUG") != NULL && o->dbg_ticks < 3)
            {
                port_log("OSB5DBG: bail joint %d (id %d) NULL\n", (int)k, (int)jid);
                o->dbg_ticks++;
            }
            return;
        }
        {
            const char *upto = getenv("SSB64_SKIN_UPTO");
            if (upto != NULL && k >= atoi(upto)) continue;
        }
        osb5_joint_frame(fp, jid, jo[k], jm[k]);
        if (pose_override_active() && jid < POSE_OVR_MAX)
        {
            PoseOvrSec *ps = pose_override_sec();
            if (ps->have[jid])
            {
                memcpy(jo[k], ps->o[jid], sizeof(jo[k]));
                memcpy(jm[k], ps->m[jid], sizeof(jm[k]));
            }
        }
    }
    if (fp->joints[0] == NULL)
    {
        if (getenv("SSB64_OSB5_DEBUG") != NULL && o->dbg_ticks < 3)
        {
            port_log("OSB5DBG: bail root joint NULL\n");
            o->dbg_ticks++;
        }
        return;
    }
    if (getenv("SSB64_NO_ROOTFRAME") != NULL) return;
    osb5_joint_frame(fp, 0, t0o, t0m);
    if (pose_override_active())
    {
        /* a joint=0 line rebases the pose at the live root (pose follows
         * the fighter); WITHOUT one the local/draw transforms cancel and
         * the pose renders at its absolute dump coordinates — immune to
         * the animation's root lean/facing (what eval renders want). */
        PoseOvrSec *ps = pose_override_sec();
        if (ps->have[0])
        {
            memcpy(t0o, ps->o[0], sizeof(t0o));
            memcpy(t0m, ps->m[0], sizeof(t0m));
        }
    }
    /* CANONICAL retarget: replace the target's frames with VIRTUAL ones —
     * mario's bone offsets walked down the canonical parent chain, each
     * joint rotated by the target joint's world rotation delta from its
     * own spawn bind (rd = R_now * R_tbind^-1). The skinning frame is
     * rd * cbind, so bind_local (computed against the canonical bind)
     * reproduces the validated mario geometry at the target's pose. */
    if (o->canonical && !pose_override_active())
    {
        static f32 rd[32][3][3];
        static f32 vjo[32][3];
        static f32 live_jo[32][3];
        f32 rd0[3][3], rdroot[3][3], tmp[3][3], t0a[3];
        f32 cp_po[3];
        static f32 rootfix[32][3][3];
        static u8 rootfix_on[32];
        s32 kk, r, have_cp;
        memcpy(live_jo, jo, sizeof(live_jo));
        osb5_mul3(rd0, t0m, o->tbind0_inv);
        memcpy(rdroot, rd0, sizeof(rdroot));
        if (osb5_on_menu_scene() && o->have_tb_cp_m)
        {
            DObj *chest = fp->joints[(s32)o->joint_ids[0]];
            if (chest != NULL && chest->parent != NULL &&
                chest->parent != DOBJ_PARENT_NULL)
            {
                f32 cpo[3], cpm[3][3], cpinv[3][3];
                osb5_dobj_frame(chest->parent, cpo, cpm);
                osb5_inv3(o->tb_cp_m, cpinv);
                osb5_mul3(rdroot, cpm, cpinv);
            }
        }
        /* the interior chain (TopN -> TransN/XRotN/YRotN -> chest) carries
         * TRANSLATE channels some figatrees animate: the appear beams the
         * body in from z=-323, crouches drop it — the vanilla mesh follows
         * them but the virtual skeleton's rigid bone offsets drop them,
         * which stranded the entry mesh off the spawn pod. Follow the
         * chest parent's world deviation from its spawn bind (the interior
         * joints are never reseated, so this stays a clean vanilla read).
         * Zero whenever the anim only rotates — validated poses unchanged. */
        {
            DObj *cj = fp->joints[(s32)o->joint_ids[0]];
            t0a[0] = t0o[0]; t0a[1] = t0o[1]; t0a[2] = t0o[2];
            have_cp = 0;
            if (cj != NULL && cj->parent != NULL && cj->parent != DOBJ_PARENT_NULL)
            {
                f32 po[3], pm[3][3];
                osb5_dobj_frame(cj->parent, po, pm);
                cp_po[0] = po[0]; cp_po[1] = po[1]; cp_po[2] = po[2];
                have_cp = 1;
                /* anchor = chest_parent_world - R0now*cint_bind; NOT
                 * written into t0o — the vert localization below must keep
                 * the real TopN frame or the DL render cancels the shift */
                for (r = 0; r < 3; r++)
                    t0a[r] = po[r] - (t0m[r][0]*o->cint_bind[0]
                                    + t0m[r][1]*o->cint_bind[1]
                                    + t0m[r][2]*o->cint_bind[2]);
            }
        }
        for (kk = 0; kk < o->njoints; kk++)
        {
            osb5_mul3(rd[kk], jm[kk], o->tbind_inv[kk]);
            osb5_mul3(tmp, rd[kk], o->cbind_m[kk]);
            memcpy(jm[kk], tmp, sizeof(tmp));
        }
        memset(rootfix_on, 0, sizeof(rootfix_on));
        if (osb5_on_menu_scene())
        {
            /* Root slots (chest, hips) hang off the interior chain by
             * rdroot-rotated chibi offsets; a leaning card pose tilts them
             * all together and the body leaves vertical. Aim each root
             * slot's offset along the live skeleton's actual direction
             * (chest parent -> that target joint) instead. */
            if (have_cp && osb5_target_is_upright_biped((s32)fp->fkind))
            {
                for (kk = 0; kk < o->njoints; kk++)
                {
                    f32 d[3], from[3], to[3], arot[3][3], fixed[3][3];
                    if ((s32)o->can_parent[kk] >= 0) continue;
                    d[0] = o->cbind_o[kk][0] - o->can_root[0];
                    d[1] = o->cbind_o[kk][1] - o->can_root[1];
                    d[2] = o->cbind_o[kk][2] - o->can_root[2];
                    for (r = 0; r < 3; r++)
                        from[r] = rdroot[r][0]*d[0] + rdroot[r][1]*d[1] + rdroot[r][2]*d[2];
                    /* both measured from the SAME anchor: t0a is where the
                     * canonical root is planted in world space, so the live
                     * direction to this slot's target joint is taken from
                     * there too (cp_po is at chest height — using it flipped
                     * the torso) */
                    to[0] = live_jo[kk][0] - t0a[0];
                    to[1] = live_jo[kk][1] - t0a[1];
                    to[2] = live_jo[kk][2] - t0a[2];
                    if (getenv("SSB64_ROOT_DBG") != NULL)
                    {
                        static s32 sRootDbg = 0;
                        if (sRootDbg++ < 60)
                            port_log("ROOTDBG kk=%d tid=%d from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f)\n",
                                     kk, (s32)o->joint_ids[kk],
                                     from[0], from[1], from[2], to[0], to[1], to[2]);
                    }
                    osb5_align3(from, to, arot);
                    memcpy(rootfix[kk], arot, sizeof(arot));
                    rootfix_on[kk] = 1;
                    /* keep the slot's child placement and skinning frame in
                     * the corrected orientation too */
                    osb5_mul3(fixed, arot, rd[kk]);
                    memcpy(rd[kk], fixed, sizeof(fixed));
                    osb5_mul3(tmp, rd[kk], o->cbind_m[kk]);
                    memcpy(jm[kk], tmp, sizeof(tmp));
                }
            }
            /* CSS figatrees bind only part of the hierarchy. Aim each
             * one-child arm segment along the target's actual live joint
             * positions while retaining the canonical segment lengths. */
            {
            s32 leg_aim = osb5_target_is_upright_biped((s32)fp->fkind);
            for (kk = 0; kk < o->njoints; kk++)
            {
                s32 cc, child = -1, nchild = 0, root = kk;
                f32 d[3], from[3], to[3], arot[3][3], fixed[3][3];
                while (o->can_parent[root] >= 0) root = o->can_parent[root];
                if (root != 0 && !leg_aim) continue;
                for (cc = 0; cc < o->njoints; cc++)
                    if ((s32)o->can_parent[cc] == kk) { child = cc; nchild++; }
                if (nchild != 1) continue;
                if (!osb5_canonical_slot_is_arm(o, kk) && !(leg_aim && root != 0)) continue;
                d[0] = o->cbind_o[child][0] - o->cbind_o[kk][0];
                d[1] = o->cbind_o[child][1] - o->cbind_o[kk][1];
                d[2] = o->cbind_o[child][2] - o->cbind_o[kk][2];
                for (r = 0; r < 3; r++)
                    from[r] = rd[kk][r][0]*d[0] + rd[kk][r][1]*d[1] + rd[kk][r][2]*d[2];
                to[0] = live_jo[child][0] - live_jo[kk][0];
                to[1] = live_jo[child][1] - live_jo[kk][1];
                to[2] = live_jo[child][2] - live_jo[kk][2];
                osb5_align3(from, to, arot);
                osb5_mul3(fixed, arot, rd[kk]);
                memcpy(rd[kk], fixed, sizeof(fixed));
                /* rd also places the child joint below. Keep this joint's
                 * skinning frame in the same corrected orientation; using
                 * the old jm while moving only the endpoint bends the
                 * blended surface into a thin rubber-hose arc. */
                osb5_mul3(tmp, rd[kk], o->cbind_m[kk]);
                memcpy(jm[kk], tmp, sizeof(tmp));
            }
            }
        }
        for (kk = 0; kk < o->njoints; kk++)
        {
            s32 pp = (s32)o->can_parent[kk];
            f32 d[3];
            if (pp < 0)
            {
                f32 base[3];
                d[0] = o->cbind_o[kk][0] - o->can_root[0];
                d[1] = o->cbind_o[kk][1] - o->can_root[1];
                d[2] = o->cbind_o[kk][2] - o->can_root[2];
                for (r = 0; r < 3; r++)
                    base[r] = rdroot[r][0]*d[0] + rdroot[r][1]*d[1] + rdroot[r][2]*d[2];
                if (rootfix_on[kk])
                {
                    f32 b2[3];
                    for (r = 0; r < 3; r++)
                        b2[r] = rootfix[kk][r][0]*base[0] + rootfix[kk][r][1]*base[1] + rootfix[kk][r][2]*base[2];
                    base[0] = b2[0]; base[1] = b2[1]; base[2] = b2[2];
                }
                for (r = 0; r < 3; r++)
                    vjo[kk][r] = t0a[r] + base[r];
            }
            else
            {
                d[0] = o->cbind_o[kk][0] - o->cbind_o[pp][0];
                d[1] = o->cbind_o[kk][1] - o->cbind_o[pp][1];
                d[2] = o->cbind_o[kk][2] - o->cbind_o[pp][2];
                for (r = 0; r < 3; r++)
                    vjo[kk][r] = vjo[pp][r] + rd[pp][r][0]*d[0] + rd[pp][r][1]*d[1] + rd[pp][r][2]*d[2];
            }
            memcpy(jo[kk], vjo[kk], sizeof(vjo[kk]));
        }
        if (getenv("SSB64_TN_DEBUG") != NULL)
        {
            static s32 sArmDbg = 0;
            if (sArmDbg < 2000 && ((sArmDbg++ % 60) == 0))
                port_log("ARMDBG t0o.y=%.1f t0a.y=%.1f chest.y=%.1f sh.y=%.1f fa.y=%.1f hd.y=%.1f\n",
                         t0o[1], t0a[1], vjo[0][1], vjo[5][1], vjo[6][1], vjo[7][1]);
        }

        /* remember the virtual seats so the LATE reseat (end of the
         * fighter tick) can re-apply them after any model-part/LOD code
         * has rewritten translates — the mid-tick write alone flickered
         * accessories between the virtual and vanilla positions on
         * alternating frames. */
        memcpy(sOsb5LateSeat[fp->player].vjo, vjo, sizeof(vjo));
        memcpy(sOsb5LateSeat[fp->player].rd, rd, sizeof(rd));
        sOsb5LateSeat[fp->player].valid = 1;
        {
        s32 pass;
        for (pass = 0; pass < 1; pass++)
        {
        /* kept-vanilla joints (samus's arm cannon) ride the REAL skeleton,
         * which is taller than the virtual chibi one — the cannon rendered
         * across the face. Re-seat each kept mapped joint onto its virtual
         * position, solved through the real parent's frame (same approach
         * as the ACC2 accessory pins below). Duplicate slots (collapsed
         * chains map two canonical joints onto one target joint) resolve
         * to the LAST slot: the canonical hand, so the fist sits in the
         * cannon. */
        for (kk = 0; kk < o->njoints; kk++)
        {
            s32 tid = (s32)o->joint_ids[kk];
            s32 kept = 1, jj;
            DObj *aj;
            f32 po[3], pm[3][3], pinv[3][3], d[3];
            if (tid <= 0 || tid >= FTPARTS_JOINT_NUM_MAX)
                continue;
            for (jj = kk + 1; jj < o->njoints; jj++)
                if ((s32)o->joint_ids[jj] == tid) { kept = 0; break; }
            if (!kept)
                continue;
            /* blanked joints are re-seated too: their own geometry is
             * hidden, but accessory CHILDREN (link's sword and shield
             * hang off the hand joints) inherit the position — without
             * this they float at the taller real skeleton's hands. Note
             * this also moves the joints game logic reads, so hit/hurt
             * ranges track the chibi skeleton — flagged for review. */
            aj = fp->joints[tid];
            if (aj == NULL || aj->parent == NULL)
                continue;
            osb5_dobj_frame(aj->parent, po, pm);
            osb5_inv3(pm, pinv);
            d[0] = vjo[kk][0] - po[0];
            d[1] = vjo[kk][1] - po[1];
            d[2] = vjo[kk][2] - po[2];
            aj->translate.vec.f.x = pinv[0][0]*d[0] + pinv[0][1]*d[1] + pinv[0][2]*d[2];
            aj->translate.vec.f.y = pinv[1][0]*d[0] + pinv[1][1]*d[1] + pinv[1][2]*d[2];
            aj->translate.vec.f.z = pinv[2][0]*d[0] + pinv[2][1]*d[1] + pinv[2][2]*d[2];
            /* menu scenes: the anim advances lazily during the display
             * walk and would rewrite this translate before the joint is
             * drawn. Bake the reseated TRS into the collision snapshot
             * and pin the joint to snapshot mode for this frame (the
             * menu unfreeze sweep skips flagged joints), so the display
             * renders the reseated position. Battle keeps its existing
             * late-reseat mechanism. */
            if (osb5_on_menu_scene() && tid < 64 &&
                (u32)fp->player < OSB5_PLAYER_SLOTS)
            {
                FTParts *spt = (FTParts *)aj->user_data.p;
                if (spt != NULL)
                {
                    extern void gmCollisionTransformMatrixAll(DObj *dobj, FTParts *parts, Mtx44f mtx);
                    gmCollisionTransformMatrixAll(aj, spt, spt->unk_dobjtrans_0x10);
                    spt->transform_update_mode = 1;
                    sOsb5MenuSeated[fp->player][tid] = 1;
                }
            }
        }
        }
        }
    }

    /* SSB64_OSB5_DEBUG=1: dump the frames the skinner actually reads for
     * the first ticks after each attach — pin down WHICH values are
     * garbage on the CSS-flash tick. */
    if (getenv("SSB64_OSB5_DEBUG") != NULL && o->dbg_ticks < 3)
    {
        DObj *rj = fp->joints[0];
        port_log("OSB5DBG: gobj=%p tick=%d root=(%.1f,%.1f,%.1f) rootTRS t=(%.1f,%.1f,%.1f) r=(%.2f,%.2f,%.2f) s=(%.2f,%.2f,%.2f) | j[0]=(%.1f,%.1f,%.1f) j[1]=(%.1f,%.1f,%.1f) j[2]=(%.1f,%.1f,%.1f)\n",
                 (void*)fighter_gobj, (int)o->dbg_ticks,
                 t0o[0], t0o[1], t0o[2],
                 rj->translate.vec.f.x, rj->translate.vec.f.y, rj->translate.vec.f.z,
                 rj->rotate.vec.f.x, rj->rotate.vec.f.y, rj->rotate.vec.f.z,
                 rj->scale.vec.f.x, rj->scale.vec.f.y, rj->scale.vec.f.z,
                 jo[0][0], jo[0][1], jo[0][2],
                 jo[1][0], jo[1][1], jo[1][2],
                 jo[2][0], jo[2][1], jo[2][2]);
        o->dbg_ticks++;
    }
    if (!osb5_frame_valid(t0m))
    {
        /* safety net only — with the memo invalidation above the root
         * frame recomputes from live TRS and should always validate. */
        return;
    }
    if (!o->t0m_attach_have)
    {
        memcpy(o->t0m_attach, t0m, sizeof(o->t0m_attach));
        o->t0m_attach_have = 1;
    }
    osb5_inv3(t0m, t0inv);
    /* Attach from the second fill on (see OSB5State.fills); afterwards
     * this doubles as the self-heal for root-DL swaps by modelpart /
     * respawn code. */
    o->fills++;
    if (o->fills >= 2 && o->mesh_dl != NULL && fp->joints[0]->dl != o->mesh_dl)
    {
        /* whatever the engine last put on the root (fresh from make, or a
         * modelpart/respawn rewrite) is the value eviction must restore */
        if (fp->joints[0]->dv != (void *)sOsb5NullDL)
        {
            o->saved_dv[0] = fp->joints[0]->dv;
            o->saved_dv_valid[0] = 1;
        }
        fp->joints[0]->dl = o->mesh_dl;
        /* mesh visible from this tick — reveal the fighter (see the
         * GOBJ_FLAG_HIDDEN set at attach) */
        fighter_gobj->flags &= ~(u32)GOBJ_FLAG_HIDDEN;
    }

    /* re-seat kept accessories: pin each root to its pinned mesh vertex,
     * inset along the vertex's inward world normal. The vertex world
     * position is skinned with the same LBS as the mesh, so the root
     * tracks the true surface through any pose. translate is
     * parent-local, so solve through the parent DObj's frame. */
    for (k = 0; k < o->naccs; k++)
    {
        s32 jid = (s32)o->accs[k].joint;
        s32 vi = (s32)o->accs[k].vert;
        DObj *aj;
        OSB5Vert *sv;
        f32 acc[3] = {0.0f, 0.0f, 0.0f};
        f32 nacc[3] = {0.0f, 0.0f, 0.0f};
        f32 po[3], pm[3][3], pinv[3][3], lo[3], wsum = 0.0f, nlen;
        s32 t, c;
        if (jid <= 0 || jid >= FTPARTS_JOINT_NUM_MAX || vi < 0 || vi >= o->nverts)
        {
            continue;
        }
        aj = fp->joints[jid];
        if (aj == NULL || aj->parent == NULL)
        {
            continue;
        }
        sv = &o->src[vi];
        for (t = 0; t < 4; t++)
        {
            f32 w = (f32)sv->w[t] / 255.0f;
            f32 *bl, *bn;
            s32 kk = sv->j[t];
            if (w <= 0.0f) continue;
            bl = o->bind_local[vi][t];
            bn = o->bind_nrm[vi][t];
            for (c = 0; c < 3; c++)
            {
                acc[c] += w * (jm[kk][c][0]*bl[0] + jm[kk][c][1]*bl[1] + jm[kk][c][2]*bl[2] + jo[kk][c]);
                nacc[c] += w * (jm[kk][c][0]*bn[0] + jm[kk][c][1]*bn[1] + jm[kk][c][2]*bn[2]);
            }
            wsum += w;
        }
        if (wsum <= 0.0f)
        {
            continue;
        }
        for (c = 0; c < 3; c++)
        {
            acc[c] /= wsum;
        }
        nlen = sqrtf(nacc[0]*nacc[0] + nacc[1]*nacc[1] + nacc[2]*nacc[2]);
        if (nlen > 1e-6f)
        {
            for (c = 0; c < 3; c++)
            {
                acc[c] -= nacc[c] * (o->accs[k].embed / nlen);
            }
        }
        osb5_dobj_frame(aj->parent, po, pm);
        osb5_inv3(pm, pinv);
        for (c = 0; c < 3; c++)
        {
            lo[c] = pinv[c][0] * (acc[0] - po[0])
                  + pinv[c][1] * (acc[1] - po[1])
                  + pinv[c][2] * (acc[2] - po[2]);
        }
        aj->translate.vec.f.x = lo[0];
        aj->translate.vec.f.y = lo[1];
        aj->translate.vec.f.z = lo[2];
        if (o->acc_orient[k] > 0.5f && o->canonical && o->acc_bind_have &&
            sOsb5LateSeat[fp->player].valid)
        {
            /* orient-follow: world rotation = chest delta * bind frame;
             * write parent-local euler in the engine's XYZ convention and
             * refresh the collision snapshot so the display sees it */
            extern float asinf(float);
            extern float atan2f(float, float);
            f32 Wt[3][3], L[3][3];
            f32 pre_x = aj->rotate.vec.f.x, pre_y = aj->rotate.vec.f.y, pre_z = aj->rotate.vec.f.z;
            FTParts *apt = (FTParts *)aj->user_data.p;
            osb5_mul3(Wt, sOsb5LateSeat[fp->player].rd[0], o->acc_bind_m[k]);
            osb5_mul3(L, pinv, Wt);
            aj->rotate.vec.f.x = atan2f(L[2][1], L[2][2]);
            aj->rotate.vec.f.y = asinf(-(L[2][0] > 1.0f ? 1.0f : (L[2][0] < -1.0f ? -1.0f : L[2][0])));
            aj->rotate.vec.f.z = atan2f(L[1][0], L[0][0]);
            aj->rotate.vec.f.x += o->acc_pitch[k];
            if (o->acc_scale[k] > 0.0f)
            {
                aj->scale.vec.f.x = o->acc_scale[k];
                aj->scale.vec.f.y = o->acc_scale[k];
                aj->scale.vec.f.z = o->acc_scale[k];
            }
            if (getenv("SSB64_ACC_DEBUG") != NULL)
            {
                static s32 sAccDbg = 0;
                FTParts *dbgpt = (FTParts *)aj->user_data.p;
                if (sAccDbg < 12000 && ((sAccDbg++ % 60) == 0))
                    port_log("ACCDBG j=%d pre=(%.2f %.2f %.2f) post=(%.2f %.2f %.2f) rd00=%.2f rd02=%.2f mode=%d\n",
                             jid, pre_x, pre_y, pre_z,
                             aj->rotate.vec.f.x, aj->rotate.vec.f.y, aj->rotate.vec.f.z,
                             sOsb5LateSeat[fp->player].rd[0][0][0],
                             sOsb5LateSeat[fp->player].rd[0][0][2],
                             dbgpt != NULL ? (int)dbgpt->transform_update_mode : -1);
            }
            if (apt != NULL && apt->transform_update_mode != 0)
            {
                extern void gmCollisionTransformMatrixAll(DObj *dobj, FTParts *parts, Mtx44f mtx);
                gmCollisionTransformMatrixAll(aj, apt, apt->unk_dobjtrans_0x10);
            }
        }
    }

    if (getenv("SSB64_SKIN_FRAMES_ONLY") != NULL) return;
    for (i = 0; i < o->nverts; i++)
    {
        OSB5Vert *v = &o->src[i];
        f32 acc[3] = {0.0f, 0.0f, 0.0f};
        f32 nacc[3] = {0.0f, 0.0f, 0.0f};
        f32 wl[3], nw[3], nl[3], nlen, wsum = 0.0f;
        s32 t, arm_weight = 0;
        if (o->canonical && osb5_on_menu_scene())
        {
            for (t = 0; t < 4; t++)
                if (osb5_canonical_slot_is_arm(o, (s32)v->j[t]))
                    arm_weight += (s32)v->w[t];
        }
        for (t = 0; t < 4; t++)
        {
            f32 w = (f32)v->w[t] / 255.0f;
            f32 *bl, *bn;
            s32 kk = v->j[t];
            /* Concentrate the broad source arm weights around their
             * dominant segment in the sharply bent CSS poses. Normalizing
             * below retains a small elbow blend without the rubber-hose
             * silhouette produced by the raw feathered weights. */
            if (arm_weight >= 128) w = w*w;
            if (w <= 0.0f) continue;
            bl = o->bind_local[i][t];
            bn = o->bind_nrm[i][t];
            acc[0] += w * (jm[kk][0][0]*bl[0] + jm[kk][0][1]*bl[1] + jm[kk][0][2]*bl[2] + jo[kk][0]);
            acc[1] += w * (jm[kk][1][0]*bl[0] + jm[kk][1][1]*bl[1] + jm[kk][1][2]*bl[2] + jo[kk][1]);
            acc[2] += w * (jm[kk][2][0]*bl[0] + jm[kk][2][1]*bl[1] + jm[kk][2][2]*bl[2] + jo[kk][2]);
            nacc[0] += w * (jm[kk][0][0]*bn[0] + jm[kk][0][1]*bn[1] + jm[kk][0][2]*bn[2]);
            nacc[1] += w * (jm[kk][1][0]*bn[0] + jm[kk][1][1]*bn[1] + jm[kk][1][2]*bn[2]);
            nacc[2] += w * (jm[kk][2][0]*bn[0] + jm[kk][2][1]*bn[1] + jm[kk][2][2]*bn[2]);
            wsum += w;
        }
        if (wsum > 0.0f)
        {
            acc[0] /= wsum; acc[1] /= wsum; acc[2] /= wsum;
        }
        wl[0] = acc[0] - t0o[0]; wl[1] = acc[1] - t0o[1]; wl[2] = acc[2] - t0o[2];
        o->vtx[i].n.ob[0] = (short)(t0inv[0][0]*wl[0] + t0inv[0][1]*wl[1] + t0inv[0][2]*wl[2]);
        o->vtx[i].n.ob[1] = (short)(t0inv[1][0]*wl[0] + t0inv[1][1]*wl[1] + t0inv[1][2]*wl[2]);
        o->vtx[i].n.ob[2] = (short)(t0inv[2][0]*wl[0] + t0inv[2][1]*wl[1] + t0inv[2][2]*wl[2]);
        /* normals: same LBS rotation, back to joint-0 local (the DL's
         * space), renormalized to s8 so lighting tracks the pose. */
        nw[0] = t0inv[0][0]*nacc[0] + t0inv[0][1]*nacc[1] + t0inv[0][2]*nacc[2];
        nw[1] = t0inv[1][0]*nacc[0] + t0inv[1][1]*nacc[1] + t0inv[1][2]*nacc[2];
        nw[2] = t0inv[2][0]*nacc[0] + t0inv[2][1]*nacc[1] + t0inv[2][2]*nacc[2];
        nlen = sqrtf(nw[0]*nw[0] + nw[1]*nw[1] + nw[2]*nw[2]);
        if (nlen > 1e-6f)
        {
            nl[0] = nw[0] * (127.0f / nlen);
            nl[1] = nw[1] * (127.0f / nlen);
            nl[2] = nw[2] * (127.0f / nlen);
            o->vtx[i].n.n[0] = (s8)nl[0];
            o->vtx[i].n.n[1] = (s8)nl[1];
            o->vtx[i].n.n[2] = (s8)nl[2];
        }
    }
}

static void osb5_reset_windows(OSB5State *o);
static OSB5State *sOsb5Loading = NULL;  /* slot whose DL is being built */

/* Fail-open eviction: screens that field several fighters under ONE player
 * index (the 1P intro spawns its whole lineup with desc.player = 0) make a
 * second injected fighter re-claim the slot while the first still lives.
 * Without this, the first fighter's joints stayed pointed at the null DL
 * and its GOBJ_FLAG_HIDDEN was never cleared — skin updates bail on the
 * owner check once the slot moves on — so it was invisible for good.
 * Restore the engine's saved draw state and reveal it: worst case it shows
 * its vanilla mesh, which beats not rendering at all. */
static void osb5_release_owner(OSB5State *o)
{
    FTStruct *fp;
    s32 k;
    if (o->owner == NULL || o->vtx == NULL)
    {
        return;
    }
    /* same double-entry liveness test the ownership gates use: pool reuse
     * means the bare pointer can name a NEW object, so the FTStruct must
     * point back at the gobj AND still be the fkind we attached to. */
    fp = ftGetStruct((GObj *)o->owner);
    if (fp == NULL || fp->fighter_gobj != o->owner || (s32)fp->fkind != o->owner_fkind)
    {
        return;
    }
    for (k = 0; k < osb5_blank_count(o); k++)
    {
        s32 jid = osb5_blank_id(o, k);
        DObj *j;
        if (jid <= 0 || jid >= FTPARTS_JOINT_NUM_MAX)
        {
            continue;
        }
        j = fp->joints[jid];
        /* only rewrite joints still holding OUR sentinels, with a saved
         * engine value to put back */
        if (j != NULL && o->saved_dv_valid[jid] &&
            (j->dv == (void *)sOsb5NullDL || j->dv == (void *)sOsb5NullDLPair))
        {
            j->dv = o->saved_dv[jid];
        }
    }
    if (fp->joints[0] != NULL)
    {
        if (o->mesh_dl != NULL && fp->joints[0]->dl == o->mesh_dl && o->saved_dv_valid[0])
        {
            fp->joints[0]->dv = o->saved_dv[0];
        }
        if (o->saved_root_nib_valid)
        {
            FTParts *rparts = (FTParts *)fp->joints[0]->user_data.p;
            if (rparts != NULL && (rparts->flags & 0xF) == 0)
            {
                rparts->flags |= o->saved_root_nib;
            }
        }
    }
    ((GObj *)o->owner)->flags &= ~(u32)GOBJ_FLAG_HIDDEN;
    port_log("OSB5: slot re-claimed while owner alive — restored vanilla mesh on fkind=%d player=%d\n",
             (int)o->owner_fkind, (int)fp->player);
}

static void osb5_load(FTStruct *fp, FILE *f)
{
    OSB5State *o = osb5_slot((s32)fp->player);
    u32 hdr[5];
    u32 njoints, nverts, ntris, tw, th;
    u8 *tex;
    Gfx *dl, *g;
    Lights1 *lt;
    u32 i, k;
    f32 jo[32][3], jm[32][3][3], jinv[32][3][3];

    if (o == NULL)
    {
        return;
    }
    if (o->owner != NULL && o->owner != fp->fighter_gobj)
    {
        osb5_release_owner(o);
    }
    memset(o->saved_dv, 0, sizeof(o->saved_dv));
    memset(o->saved_dv_valid, 0, sizeof(o->saved_dv_valid));
    o->saved_root_nib_valid = 0;
    sOsb5Loading = o;
    /* a fighter is spawned many times per session (select screens,
     * respawns, results); each attach must start with a fresh window
     * table (for THIS slot) or the 512-slot cap fills after a few
     * attaches and the new mesh renders empty. */
    osb5_reset_windows(o);

    fread(hdr, 4, 5, f);
    njoints = hdr[0]; nverts = hdr[1]; ntris = hdr[2]; tw = hdr[3]; th = hdr[4];
    if (njoints > 32) { port_log("OSB5: too many joints\n"); return; }

    o->njoints = (s32)njoints;
    o->nverts = (s32)nverts;
    o->nblank = 0;
    o->naccs = 0;
    o->have_tbnd = 0;
    /* canonical is set ONLY by a CAN1 section. Without this reset, a
     * CLASSIC bundle attached into a slot that previously wore a
     * canonical character (CSS hover previews cycle characters through
     * the same player slot) inherits canonical=1 plus the previous
     * character's joint mapping, and the per-tick canonical reseat then
     * rewrites the classic rig's joint translates with stale virtual
     * seats — the select-card bent/hunched pose after dragging across
     * canonical tiles. */
    o->canonical = 0;
    sOsb5LateSeat[fp->player].valid = 0;
    for (k = 0; k < 8; k++) { o->acc_pitch[k] = 0.0f; o->acc_orient[k] = 0.0f; o->acc_scale[k] = 0.0f; }
    o->fit_scale = 1.0f;
    o->scl_applied = 0.0f;
    fread(o->joint_ids, 4, njoints, f);

    /* Fail-open on a skeleton mismatch: every tracked joint must exist on
     * the fighter actually spawning. A bundle conformed for a different
     * base (e.g. the default mario-variant .osb injected onto samus) would
     * otherwise blank this fighter's body, then bail out of every skin
     * update on the missing joint — and since the deferred attach hides
     * the GObj until the first successful fill, the fighter stayed
     * INVISIBLE forever. Abort before touching anything: the fighter
     * plays with its vanilla mesh and the log names the bad joint. */
    for (i = 0; i < njoints; i++)
    {
        u32 jid = o->joint_ids[i];
        if (jid >= FTPARTS_JOINT_NUM_MAX || fp->joints[jid] == NULL)
        {
            port_log("OSB5: bundle/skeleton mismatch — joint id %u absent on fkind=%d; injection aborted (vanilla mesh kept)\n",
                     jid, (int)fp->fkind);
            return;
        }
    }

    tex = (u8 *)malloc(tw * th * 2);
    fread(tex, 2, tw * th, f);

    o->src = (OSB5Vert *)malloc(sizeof(OSB5Vert) * nverts);
    fread(o->src, sizeof(OSB5Vert), nverts, f);

    /* inverse bind: prefer the bind skeleton embedded in the bundle
     * (the pose the verts were authored against). Binding to the LIVE
     * pose only works if the mesh attaches at exactly that pose — in
     * real play it attaches mid entry-animation and limbs fly off. */
    {
        long vpos = ftell(f);
        char tag[4] = {0, 0, 0, 0};
        s32 have_bind = 0;
        s32 have_tag;
        fseek(f, (long)ntris * 8, SEEK_CUR);
        have_tag = (fread(tag, 1, 4, f) == 4);
        if (have_tag && tag[0] == 'B' && tag[1] == 'I' && tag[2] == 'N' && tag[3] == 'D')
        {
            f32 fb[12];
            have_bind = 1;
            for (k = 0; k < njoints; k++)
            {
                if (fread(fb, 4, 12, f) != 12) { have_bind = 0; break; }
                jo[k][0] = fb[0]; jo[k][1] = fb[1]; jo[k][2] = fb[2];
                jm[k][0][0] = fb[3]; jm[k][0][1] = fb[4]; jm[k][0][2] = fb[5];
                jm[k][1][0] = fb[6]; jm[k][1][1] = fb[7]; jm[k][1][2] = fb[8];
                jm[k][2][0] = fb[9]; jm[k][2][1] = fb[10]; jm[k][2][2] = fb[11];
                osb5_inv3(jm[k], jinv[k]);
            }
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        if (have_tag && tag[0] == 'B' && tag[1] == 'L' && tag[2] == 'N' && tag[3] == 'K')
        {
            u32 nb = 0;
            if (fread(&nb, 4, 1, f) == 1 && nb <= 64)
            {
                if (fread(o->blank_ids, 4, nb, f) == nb)
                {
                    o->nblank = (s32)nb;
                    port_log("OSB5: blank list of %u joints\n", nb);
                }
            }
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        if (have_tag && tag[0] == 'A' && tag[1] == 'C' && tag[2] == 'C' && tag[3] == '2')
        {
            u32 na = 0;
            if (fread(&na, 4, 1, f) == 1 && na <= 8)
            {
                for (k = 0; k < na; k++)
                {
                    u32 aids[2];
                    f32 ae;
                    if (fread(aids, 4, 2, f) != 2 || fread(&ae, 4, 1, f) != 1)
                    {
                        break;
                    }
                    o->accs[k].joint = aids[0];
                    o->accs[k].vert = aids[1];
                    o->accs[k].embed = ae;
                }
                o->naccs = (s32)k;
                port_log("OSB5: %d accessory vertex pin(s)\n", o->naccs);
            }
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        if (have_tag && tag[0] == 'A' && tag[1] == 'C' && tag[2] == 'C' && tag[3] == '3')
        {
            /* per-pin: pitch offset + orient flag */
            u32 np = 0;
            if (fread(&np, 4, 1, f) == 1 && np <= 8)
            {
                u32 pk;
                for (pk = 0; pk < np; pk++)
                {
                    f32 pp2[3];
                    if (fread(pp2, 4, 3, f) != 3) break;
                    o->acc_pitch[pk] = pp2[0];
                    o->acc_orient[pk] = pp2[1];
                    o->acc_scale[pk] = pp2[2];
                }
                port_log("OSB5: %u accessory pitch/orient entries\n", np);
            }
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        if (have_tag && tag[0] == 'C' && tag[1] == 'A' && tag[2] == 'N' && tag[3] == '1')
        {
            s32 pars[32];
            if (have_bind && njoints <= 32 &&
                fread(o->can_root, 4, 3, f) == 3 &&
                fread(pars, 4, njoints, f) == (size_t)njoints)
            {
                for (k = 0; k < njoints; k++)
                {
                    o->can_parent[k] = (s8)pars[k];
                    memcpy(o->cbind_o[k], jo[k], sizeof(o->cbind_o[k]));
                    memcpy(o->cbind_m[k], jm[k], sizeof(o->cbind_m[k]));
                }
                o->canonical = 1;
                /* accessory lever-arm scale: canonical chest height over
                 * target chest height (both above their ground anchors) */
                o->can_scale = 1.0f;
                {
                    f32 ch = jo[0][1] - o->can_root[1];   /* slot 0 = chest */
                    f32 tjo_[3], tjm_[3][3];
                    if (fp->joints[(s32)o->joint_ids[0]] != NULL)
                    {
                        osb5_joint_frame(fp, (s32)o->joint_ids[0], tjo_, tjm_);
                        if (tjo_[1] > 1.0f && ch > 1.0f)
                        {
                            f32 t0o_[3], t0m_[3][3];
                            osb5_joint_frame(fp, 0, t0o_, t0m_);
                            if (tjo_[1] - t0o_[1] > 1.0f)
                                o->can_scale = ch / (tjo_[1] - t0o_[1]);
                        }
                    }
                }
                port_log("OSB5: CANONICAL retarget (%d joints, scale %.2f)\n",
                         (int)njoints, o->can_scale);
            }
            have_tag = (fread(tag, 1, 4, f) == 4);
        if (have_tag && tag[0] == 'T' && tag[1] == 'B' && tag[2] == 'N' && tag[3] == 'D')
        {
            /* baked target bind (battle-spawn frames from the pipeline's
             * skeleton dumps): slot origin+basis per joint, TopN, chest
             * parent + chest origins, accessory bases */
            s32 ok2 = 1;
            f32 slot_o[3];
            for (k = 0; k < njoints && ok2; k++)
            {
                ok2 = fread(slot_o, 4, 3, f) == 3 &&
                      fread(o->tb_slot_m[k], 4, 9, f) == 9;
            }
            ok2 = ok2 && fread(o->tb_top_o, 4, 3, f) == 3 &&
                  fread(o->tb_top_m, 4, 9, f) == 9 &&
                  fread(o->tb_cp_o, 4, 3, f) == 3 &&
                  fread(o->tb_chest_o, 4, 3, f) == 3;
            if (ok2)
            {
                u32 na2 = 0;
                if (fread(&na2, 4, 1, f) == 1 && na2 <= 8)
                {
                    u32 a2;
                    for (a2 = 0; a2 < na2 && ok2; a2++)
                        ok2 = fread(o->tb_acc_m[a2], 4, 9, f) == 9;
                }
                else ok2 = 0;
            }
            o->have_tbnd = (u8)(ok2 ? 1 : 0);
            port_log("OSB5: baked target bind %s\n", ok2 ? "loaded" : "TRUNCATED");
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        if (have_tag && tag[0] == 'C' && tag[1] == 'P' && tag[2] == 'M' && tag[3] == '1')
        {
            o->have_tb_cp_m = (u8)(fread(o->tb_cp_m, 4, 9, f) == 9);
            port_log("OSB5: chest-parent bind %s\n",
                     o->have_tb_cp_m ? "loaded" : "TRUNCATED");
            have_tag = (fread(tag, 1, 4, f) == 4);
        }
        }
        if (have_tag && tag[0] == 'S' && tag[1] == 'C' && tag[2] == 'A' && tag[3] == 'L')
        {
            f32 fs = 1.0f;
            if (fread(&fs, 4, 1, f) == 1 && fs >= 0.5f && fs <= 1.0f)
            {
                o->fit_scale = fs;
                port_log("OSB5: fit scale x%.3f\n", fs);
            }
        }
        fseek(f, vpos, SEEK_SET);
        if (have_bind)
        {
            port_log("OSB5: using embedded bind skeleton\n");
        }
        else
        {
            for (k = 0; k < njoints; k++)
            {
                s32 jid = (s32)o->joint_ids[k];
                if (fp->joints[jid] == NULL) { port_log("OSB5: missing joint %d\n", jid); return; }
                osb5_joint_frame(fp, jid, jo[k], jm[k]);
                osb5_inv3(jm[k], jinv[k]);
            }
        }
    }
    o->bind_local = malloc(sizeof(*o->bind_local) * nverts);
    for (i = 0; i < nverts; i++)
    {
        OSB5Vert *v = &o->src[i];
        s32 t;
        for (t = 0; t < 4; t++)
        {
            s32 kk = v->j[t];
            f32 d0 = v->x - jo[kk][0], d1 = v->y - jo[kk][1], d2 = v->z - jo[kk][2];
            o->bind_local[i][t][0] = jinv[kk][0][0]*d0 + jinv[kk][0][1]*d1 + jinv[kk][0][2]*d2;
            o->bind_local[i][t][1] = jinv[kk][1][0]*d0 + jinv[kk][1][1]*d1 + jinv[kk][1][2]*d2;
            o->bind_local[i][t][2] = jinv[kk][2][0]*d0 + jinv[kk][2][1]*d1 + jinv[kk][2][2]*d2;
        }
    }
    o->bind_nrm = malloc(sizeof(*o->bind_nrm) * nverts);
    for (i = 0; i < nverts; i++)
    {
        OSB5Vert *v = &o->src[i];
        f32 n0 = (f32)v->n[0], n1 = (f32)v->n[1], n2 = (f32)v->n[2];
        s32 t;
        for (t = 0; t < 4; t++)
        {
            s32 kk = v->j[t];
            o->bind_nrm[i][t][0] = jinv[kk][0][0]*n0 + jinv[kk][0][1]*n1 + jinv[kk][0][2]*n2;
            o->bind_nrm[i][t][1] = jinv[kk][1][0]*n0 + jinv[kk][1][1]*n1 + jinv[kk][1][2]*n2;
            o->bind_nrm[i][t][2] = jinv[kk][2][0]*n0 + jinv[kk][2][1]*n1 + jinv[kk][2][2]*n2;
        }
    }

    if (o->canonical && o->have_tbnd)
    {
        /* baked target bind: independent of the pose the fighter holds
         * at inject — the CSS/results screens re-make fighters mid
         * victory-pose and live capture there deformed every render */
        f32 d[3];
        s32 r2;
        for (k = 0; k < njoints; k++)
            osb5_inv3(o->tb_slot_m[k], o->tbind_inv[k]);
        osb5_inv3(o->tb_top_m, o->tbind0_inv);
        for (k = 0; k < o->naccs && k < 8; k++)
            memcpy(o->acc_bind_m[k], o->tb_acc_m[k], sizeof(o->acc_bind_m[k]));
        o->acc_bind_have = 1;
        d[0] = o->tb_cp_o[0] - o->tb_top_o[0];
        d[1] = o->tb_cp_o[1] - o->tb_top_o[1];
        d[2] = o->tb_cp_o[2] - o->tb_top_o[2];
        for (r2 = 0; r2 < 3; r2++)
            o->cint_bind[r2] = o->tbind0_inv[r2][0]*d[0]
                             + o->tbind0_inv[r2][1]*d[1]
                             + o->tbind0_inv[r2][2]*d[2];
        {
            f32 ch = jo[0][1] - o->can_root[1];
            f32 th = o->tb_chest_o[1] - o->tb_top_o[1];
            if (ch > 1.0f && th > 1.0f)
                o->can_scale = ch / th;
        }
        port_log("OSB5: canonical bind from TBND\n");
    }
    else if (o->canonical)
    {
        f32 tjo[3], tjm[3][3];
        for (k = 0; k < njoints; k++)
        {
            s32 jid = (s32)o->joint_ids[k];
            if (fp->joints[jid] == NULL)
            {
                port_log("OSB5: canonical: target joint %d missing, disabling\n", jid);
                o->canonical = 0;
                break;
            }
            osb5_joint_frame(fp, jid, tjo, tjm);
            osb5_inv3(tjm, o->tbind_inv[k]);
        }
        if (o->canonical)
        {
            s32 jid2, kk2;
            osb5_joint_frame(fp, 0, tjo, tjm);
            osb5_inv3(tjm, o->tbind0_inv);
            o->acc_bind_have = 0;
            {
                s32 ak;
                for (ak = 0; ak < o->naccs && ak < 8; ak++)
                {
                    s32 ajid = (s32)o->accs[ak].joint;
                    DObj *adj = (ajid > 0 && ajid < FTPARTS_JOINT_NUM_MAX) ? fp->joints[ajid] : NULL;
                    f32 ao_[3];
                    if (adj != NULL)
                        osb5_dobj_frame(adj, ao_, o->acc_bind_m[ak]);
                }
                o->acc_bind_have = 1;
            }
            o->cint_bind[0] = o->cint_bind[1] = o->cint_bind[2] = 0.0f;
            {
                DObj *cj = fp->joints[(s32)o->joint_ids[0]];
                if (cj != NULL && cj->parent != NULL && cj->parent != DOBJ_PARENT_NULL)
                {
                    f32 po[3], pm[3][3], d[3];
                    s32 r2;
                    osb5_dobj_frame(cj->parent, po, pm);
                    d[0] = po[0] - tjo[0];
                    d[1] = po[1] - tjo[1];
                    d[2] = po[2] - tjo[2];
                    for (r2 = 0; r2 < 3; r2++)
                        o->cint_bind[r2] = o->tbind0_inv[r2][0]*d[0]
                                         + o->tbind0_inv[r2][1]*d[1]
                                         + o->tbind0_inv[r2][2]*d[2];
                }
            }
        }
    }
    if (o->canonical)
    {
        s32 jid2, kk2;
        /* spawn-translate snapshot for every UNMAPPED registry joint:
         * accessory mounts (link's sword/shield/sheath joints) keep
         * static local offsets sized for the tall vanilla body. Each
         * tick they are SET to snapshot*scale — a per-tick multiply
         * compounded on joints the animation never rewrites and
         * collapsed the gear onto its parents. */
        for (jid2 = 0; jid2 < 64; jid2++)
        {
            o->can_snap_have[jid2] = 0;
            o->can_interior[jid2] = 0;
        }
        /* interior = ancestors of mapped joints (walk each mapped
         * joint's DObj parent chain to the root, flagging unmapped
         * nodes on the way) */
        for (kk2 = 0; kk2 < o->njoints && kk2 < 32; kk2++)
        {
            s32 tid2 = (s32)o->joint_ids[kk2];
            s32 ptid = -1, pk;
            DObj *walk, *stopj = NULL;
            o->can_chainoff[kk2][0] = o->can_chainoff[kk2][1] = o->can_chainoff[kk2][2] = 0.0f;
            if (tid2 <= 0 || tid2 >= FTPARTS_JOINT_NUM_MAX || fp->joints[tid2] == NULL)
                continue;
            pk = (s32)o->can_parent[kk2];
            if (pk >= 0)
            {
                ptid = (s32)o->joint_ids[pk];
                if (ptid > 0 && ptid < FTPARTS_JOINT_NUM_MAX)
                    stopj = fp->joints[ptid];
            }
            else stopj = fp->joints[0];
            for (walk = fp->joints[tid2]->parent;
                 walk != NULL && walk != DOBJ_PARENT_NULL && walk != stopj;
                 walk = walk->parent)
            {
                /* world-space approximation: nub rotations at bind are
                 * near-identity, so raw translate sums suffice */
                o->can_chainoff[kk2][0] += walk->translate.vec.f.x;
                o->can_chainoff[kk2][1] += walk->translate.vec.f.y;
                o->can_chainoff[kk2][2] += walk->translate.vec.f.z;
            }
        }
        if (getenv("SSB64_NO_INTERIOR") == NULL)
        for (kk2 = 0; kk2 < o->njoints; kk2++)
        {
            s32 tid2 = (s32)o->joint_ids[kk2];
            DObj *walk;
            if (tid2 <= 0 || tid2 >= FTPARTS_JOINT_NUM_MAX) continue;
            if (fp->joints[tid2] == NULL) continue;
            for (walk = fp->joints[tid2]->parent;
                 walk != NULL && walk != DOBJ_PARENT_NULL;
                 walk = walk->parent)
            {
                s32 wj;
                for (wj = 1; wj < FTPARTS_JOINT_NUM_MAX && wj < 64; wj++)
                    if (fp->joints[wj] == walk) { o->can_interior[wj] = 1; break; }
            }
        }
        for (jid2 = 1; jid2 < FTPARTS_JOINT_NUM_MAX && jid2 < 64; jid2++)
        {
            s32 m = 0;
            if (fp->joints[jid2] == NULL) continue;
            for (kk2 = 0; kk2 < o->njoints; kk2++)
                if ((s32)o->joint_ids[kk2] == jid2) { m = 1; break; }
            if (m) continue;
            o->can_snap[jid2][0] = fp->joints[jid2]->translate.vec.f.x;
            o->can_snap[jid2][1] = fp->joints[jid2]->translate.vec.f.y;
            o->can_snap[jid2][2] = fp->joints[jid2]->translate.vec.f.z;
            o->can_snap_have[jid2] = 1;
        }
    }

    /* live Vtx array + one DL on joint 0; st/normals static, ob updated
     * per frame by port_osb5_skin_update(). */
    o->vtx = (Vtx *)malloc(sizeof(Vtx) * nverts);
    for (i = 0; i < nverts; i++)
    {
        OSB5Vert *v = &o->src[i];
        o->vtx[i].n.ob[0] = 0; o->vtx[i].n.ob[1] = 0; o->vtx[i].n.ob[2] = 0;
        o->vtx[i].n.flag = 0;
        o->vtx[i].n.tc[0] = v->s; o->vtx[i].n.tc[1] = v->t;
        o->vtx[i].n.n[0] = v->n[0]; o->vtx[i].n.n[1] = v->n[1]; o->vtx[i].n.n[2] = v->n[2];
        o->vtx[i].n.a = 0xFF;
    }

    dl = (Gfx *)malloc(sizeof(Gfx) * (32 + ntris * 2 + nverts / 8));
    g = dl;
    {
        static Lights1 sOsb5Lights = gdSPDefLights1(145, 145, 145, 255, 255, 255, 45, 95, 70);
        u16 *traw = (u16 *)malloc(ntris * 8);
        fread(traw, 8, ntris, f);

        gDPPipeSync(g++);
        gSPClearGeometryMode(g++, G_TEXTURE_GEN | G_CULL_BOTH);
        gSPSetGeometryMode(g++, G_LIGHTING | G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
        gSPSetLights1(g++, sOsb5Lights);
        gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_PASS2);
        gDPSetTexturePersp(g++, G_TP_PERSP);
        gDPSetTextureFilter(g++, G_TF_BILERP);
        gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTextureImage(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, tw, tex);
        gDPSetTile(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (tw * 2) / 8, 0,
                   G_TX_LOADTILE, 0, G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
        gDPLoadSync(g++);
        gDPLoadTile(g++, G_TX_LOADTILE, 0, 0, (tw - 1) << 2, (th - 1) << 2);
        gDPPipeSync(g++);
        gDPSetTile(g++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (tw * 2) / 8, 0,
                   G_TX_RENDERTILE, 0, G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
        gDPSetTileSize(g++, G_TX_RENDERTILE, 0, 0, (tw - 1) << 2, (th - 1) << 2);

        /* window the verts 30 at a time; tris indexed within windows.
         * Reorder: emit tris grouped by vertex window of their max index.
         * Simple approach: process tris in order, filling windows. */
        {
            u32 done = 0;
            while (done < ntris)
            {
                s32 *map = (s32 *)malloc(sizeof(s32) * nverts);
                u32 count = 0, twin = 0;
                u32 start = done;
                for (i = 0; i < nverts; i++) map[i] = -1;
                while (done < ntris)
                {
                    u16 *t3 = &traw[done * 4];
                    u32 need = 0, t;
                    for (t = 0; t < 3; t++) if (map[t3[t]] < 0) need++;
                    if (count + need > 30) break;
                    for (t = 0; t < 3; t++)
                        if (map[t3[t]] < 0) map[t3[t]] = (s32)count++;
                    done++;
                    twin++;
                }
                if (twin == 0) break;
                /* window vtx list */
                {
                    static Vtx *winptrs[512];
                    static u32 nwin = 0;
                    /* Zero the window: the DL references it immediately,
                     * but port_osb5_copy_windows only fills it on the next
                     * fighter tick — raw malloc garbage here IS the CSS
                     * one-frame "exploding mesh" flash on custom tiles
                     * (giant triangles textured with the incoming
                     * character's atlas). Zeroed verts degenerate to a
                     * single invisible point instead. */
                    Vtx *wv = (Vtx *)malloc(sizeof(Vtx) * count);
                    memset(wv, 0, sizeof(Vtx) * count);
                    (void)winptrs; (void)nwin;
                    /* record mapping so skin update can copy: simpler —
                     * keep per-window index list and copy in update. */
                    {
                        /* store window remap into a global table */
                        extern void osb5_add_window(Vtx *wv, s32 *map_idx, u32 count);
                        s32 *mi = (s32 *)malloc(sizeof(s32) * count);
                        for (i = 0; i < nverts; i++)
                            if (map[i] >= 0) mi[map[i]] = (s32)i;
                        osb5_add_window(wv, mi, count);
                    }
                    for (i = 0; i < count; i++) { }
                    gSPVertex(g++, wv, count, 0);
                    for (i = start; i < done; i++)
                    {
                        u16 *t3 = &traw[i * 4];
                        gSP1Triangle(g++, map[t3[0]], map[t3[1]], map[t3[2]], 0);
                    }
                }
            }
        }
        free(traw);
    }
    gDPPipeSync(g++);
    gSPSetGeometryMode(g++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPEndDisplayList(g++);

    /* attach: joint 0 renders the whole mesh; blank exactly the joints the
     * bundle REPLACES (its skinned skeleton). Accessory joints (Link's
     * sword/shield, arm cannons, ...) are not in the bundle and keep their
     * vanilla DLs and modelpart behavior 1:1. */
    {
        s32 jj, k;
        gSPEndDisplayList(&sOsb5NullDL[0]);
        if (getenv("SSB64_NO_BLANK") != NULL) jj = FTPARTS_JOINT_NUM_MAX; else jj = 1;
        for (; jj < FTPARTS_JOINT_NUM_MAX; jj++)
        {
            if (fp->joints[jj] == NULL)
            {
                continue;
            }
            for (k = 0; k < osb5_blank_count(o); k++)
            {
                if (osb5_blank_id(o, (s32)k) == jj)
                {
                    osb5_blank_joint(o, fp, jj);
                    break;
                }
            }
        }
        if (getenv("SSB64_NO_ROOTDL") == NULL)
        {
            /* the mesh must draw via the plain-DL path */
            FTParts *rparts = (FTParts *)fp->joints[0]->user_data.p;
            /* keep the fighter matrix func (kind 75) ACTIVE on the root:
             * it rebuilds TopN's matrix every frame. Clearing the nibble
             * put the DL on the generic path's lazily-rebuilt matrix
             * cache, which missed facing turns — the mesh stayed at
             * spawn facing while the fighter turned (SSB64_CLEAR_NIB
             * restores the old behavior for comparison). */
            if (rparts != NULL && (rparts->flags & 0xF) != 0 &&
                getenv("SSB64_CLEAR_NIB") != NULL)
            {
                o->saved_root_nib = (u8)(rparts->flags & 0xF);
                o->saved_root_nib_valid = 1;
                rparts->flags &= ~0xF;
            }
            /* deferred: the first VALID skin update attaches (see
             * OSB5State.mesh_dl) — never draw the zero-initialized vtx. */
            o->mesh_dl = dl;
        }
    }
    o->owner = fp->fighter_gobj;
    o->owner_fkind = (s32)fp->fkind;
    o->owner_char = sInjectCharIdx;
    o->dbg_ticks = 0;
    o->fills = 0;
    /* Hide the WHOLE fighter until the mesh attaches (fill #2): the mesh
     * is deliberately unattached for the make tick, but every unblanked
     * vanilla joint — Link's sword/shield, DK's tie, any kept accessory —
     * would still draw, floating alone for one frame. GOBJ_FLAG_HIDDEN
     * skips proc_display entirely (same mechanism sc1pintro uses for its
     * staged fighter reveal); skin_update clears it when the mesh DL
     * attaches, so the fighter pops in complete and correctly posed. */
    fp->fighter_gobj->flags |= GOBJ_FLAG_HIDDEN;
    /* Copy the attach-time skin fill into the windows NOW: the fighter can
     * draw this same tick, before ftMainProcParams next runs copy_windows,
     * and the DL renders the windows — not o->vtx. (The skin update below
     * fills vtx with the rest pose; without this copy the first frame drew
     * whatever the freshly allocated windows held.) */
    port_osb5_skin_update(fp->fighter_gobj);
    {
        extern void port_osb5_copy_windows(void);
        port_osb5_copy_windows();
    }
    port_log("OSB5: skinned mesh attached (%u verts, %u tris, %u joints) player=%d fkind=%d gobj=%p\n",
             nverts, ntris, njoints, (int)fp->player, (int)fp->fkind, (void *)fp->fighter_gobj);
}

/* window table: skin update copies skinned verts into window arrays.
 * Windows are tagged with the slot whose DL owns them so per-slot
 * reloads (each fighter respawn) don't orphan other slots' windows. */
typedef struct { Vtx *wv; s32 *idx; u32 count; OSB5State *slot; } OSB5Window;
/* Each skinned mesh needs ~300 windows (30-vert gSPVertex batches), and a
 * demo roster runs several meshes at once — 512 silently truncated the
 * SECOND character's later windows (face detail), leaving those verts at
 * the origin. Size for a full 4-fighter roster with headroom. */
#define OSB5_MAX_WINDOWS 4096
static OSB5Window sOsb5Windows[OSB5_MAX_WINDOWS];
static u32 sOsb5NumWindows = 0;

static void osb5_reset_windows(OSB5State *o)
{
    u32 w, out = 0;
    for (w = 0; w < sOsb5NumWindows; w++)
    {
        if (sOsb5Windows[w].slot == o)
        {
            free(sOsb5Windows[w].idx);   /* wv referenced by the old DL; leak it */
            continue;
        }
        sOsb5Windows[out++] = sOsb5Windows[w];
    }
    sOsb5NumWindows = out;
}

void osb5_add_window(Vtx *wv, s32 *map_idx, u32 count)
{
    if (sOsb5NumWindows >= OSB5_MAX_WINDOWS)
    {
        port_log("OSB5: window table FULL (%d) — mesh will render incomplete\n",
                 (int)sOsb5NumWindows);
        return;
    }
    if (1)
    {
        sOsb5Windows[sOsb5NumWindows].wv = wv;
        sOsb5Windows[sOsb5NumWindows].idx = map_idx;
        sOsb5Windows[sOsb5NumWindows].count = count;
        sOsb5Windows[sOsb5NumWindows].slot = sOsb5Loading;
        sOsb5NumWindows++;
    }
}

void port_osb5_copy_windows(void)
{
    u32 w, i;
    for (w = 0; w < sOsb5NumWindows; w++)
    {
        OSB5Window *win = &sOsb5Windows[w];
        OSB5State *o = win->slot;
        if (o == NULL || o->vtx == NULL)
        {
            continue;
        }
        for (i = 0; i < win->count; i++)
        {
            win->wv[i] = o->vtx[win->idx[i]];
        }
    }
}

/* ------------------------------------------------------------------ */
/*  OpenSmash UI assets: dump / inject the 2D sprites tied to a        */
/*  fighter kind — CSS portrait, CSS name text, in-battle stock icon.  */
/*  Dump (SSB64_DUMP_SPRITES=<dir>) writes each sprite's decoded RGBA  */
/*  plus a .json sidecar so the pipeline can match dimensions and      */
/*  style. All sprites here are CI4/CI8 with RGBA5551 TLUTs.           */
/* ------------------------------------------------------------------ */
#include <PR/sp.h>

static u32 port_sprite_px(u16 texel5551)
{
    u32 r = (texel5551 >> 11) & 0x1F, g = (texel5551 >> 6) & 0x1F;
    u32 b = (texel5551 >> 1) & 0x1F, a = texel5551 & 1;
    return (r << 3 | r >> 2) | ((g << 3 | g >> 2) << 8) | ((b << 3 | b >> 2) << 16) |
           ((a ? 0xFFu : 0x00u) << 24);
}

extern void portFixupSprite(void *sprite);
extern void portFixupBitmapArray(void *bitmaps, s32 count);

/* Texel bytes are still in the file's blanket-u32-swapped state before
 * the first draw; logical byte i lives at (i & ~3) | (3 - (i & 3)). */
static u8 port_ui_texel(u8 *buf, s32 i)
{
    return buf[(i & ~3) | (3 - (i & 3))];
}

void port_ui_dump_sprite(const char *dir, const char *name, Sprite *spr)
{
    char path[1024];
    FILE *f;
    Bitmap *bms;
    u8 *tlut;
    s32 b;

    if (spr == NULL) return;
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    tlut = (u8 *)PORT_RESOLVE(spr->LUT);

    snprintf(path, sizeof path, "%s/%s.json", dir, name);
    f = fopen(path, "w");
    if (f != NULL)
    {
        fprintf(f, "{\"draw_w\": %d, \"draw_h\": %d, \"bmfmt\": %d, \"bmsiz\": %d,\n"
                   " \"nbitmaps\": %d, \"bmheight\": %d, \"bmheight_real\": %d,\n"
                   " \"ntlut\": %d, \"start_tlut\": %d, \"attr\": %d, \"bitmaps\": [\n",
                spr->width, spr->height, spr->bmfmt, spr->bmsiz, spr->nbitmaps,
                spr->bmheight, spr->bmHreal, spr->nTLUT, spr->startTLUT, spr->attr);
        for (b = 0; b < spr->nbitmaps; b++)
        {
            fprintf(f, "  {\"width\": %d, \"width_img\": %d, \"s\": %d, \"t\": %d,"
                       " \"actual_h\": %d, \"lut_off\": %d}%s\n",
                    bms[b].width, bms[b].width_img, bms[b].s, bms[b].t,
                    bms[b].actualHeight, bms[b].LUToffset, (b + 1 < spr->nbitmaps) ? "," : "");
        }
        fprintf(f, "]}\n");
        fclose(f);
    }
    /* raw texel bytes per bitmap, concatenated, exactly as in memory */
    snprintf(path, sizeof path, "%s/%s.bufs", dir, name);
    f = fopen(path, "wb");
    if (f != NULL)
    {
        for (b = 0; b < spr->nbitmaps; b++)
        {
            u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
            s32 bpt = (spr->bmsiz == G_IM_SIZ_16b) ? 16 : (spr->bmsiz == G_IM_SIZ_8b) ? 8 :
                      (spr->bmsiz == G_IM_SIZ_32b) ? 32 : 4;
            s32 nbytes = (bms[b].width_img * bms[b].actualHeight * bpt) / 8;
            if (buf != NULL) fwrite(buf, 1, nbytes, f);
        }
        fclose(f);
    }
    if (tlut != NULL && spr->nTLUT > 0)
    {
        snprintf(path, sizeof path, "%s/%s.tlut", dir, name);
        f = fopen(path, "wb");
        if (f != NULL)
        {
            fwrite(tlut, 2, spr->nTLUT, f);
            fclose(f);
        }
    }
    port_log("OSBUI: dumped %s (fmt=%d siz=%d nbm=%d ntlut=%d)\n",
             name, spr->bmfmt, spr->bmsiz, spr->nbitmaps, spr->nTLUT);
}

/* Write a LOGICAL pixel canvas into a sprite whose geometry varies per
 * fighter (the name sprites are 40/48/64 wide depending on character).
 * canvas is canvas_w wide, rows top-down; one byte per texel (IA8: value
 * used as-is; I4: high nibble used). The bitmap data is force-converted
 * to the port's linear texel state first and written linear, so repaints
 * (roster page flips) stay correct — see port_ui_write_canvas_fit. */
extern void portFixupSpriteBitmapData(void *sprite, void *bitmaps);
extern unsigned int portRelocRegisterPointer(void *ptr);
extern void portMarkSyntheticSprite(void *sprite, void *bitmaps, unsigned int nbitmaps, void **bufs);

/* Vanilla sizes each fighter's name sprite to its name (FOX 32 ... DK 72).
 * When injected text is wider than the tile fighter's sprite, GROW the
 * sprite (a wider synthetic bitmap clone) rather than nearest-resampling
 * the canvas down — the resample is what shredded long injected names.
 * The original geometry is remembered so the vanilla-restore path can
 * revert before putting pristine texels back. */
typedef struct
{
    Sprite *spr;
    u32 orig_bitmap;
    u16 orig_w;
} UIWiden;
static UIWiden sUIWidens[64];
static s32 sNUIWidens = 0;

static UIWiden *port_ui_widen_find(Sprite *spr)
{
    s32 i;
    for (i = 0; i < sNUIWidens; i++)
    {
        if (sUIWidens[i].spr == spr) return &sUIWidens[i];
    }
    return NULL;
}

static void port_ui_unwiden(Sprite *spr)
{
    UIWiden *w = (spr != NULL) ? port_ui_widen_find(spr) : NULL;
    if (w == NULL) return;
    /* the synthetic clone + buffer are intentionally leaked: they may be
     * referenced by a privatized card sprite that outlives the tile */
    spr->bitmap = w->orig_bitmap;
    spr->width = w->orig_w;
    *w = sUIWidens[--sNUIWidens];
}

static Bitmap *port_ui_widen(Sprite *spr, Bitmap *bms, s32 want_w)
{
    Bitmap *clone;
    u8 *src, *dst;
    void *bufs[1];
    s32 y, old_stride, h, stride;

    if (spr->nbitmaps != 1 || spr->bmsiz != G_IM_SIZ_8b)
    {
        return bms;             /* only the single-bitmap IA8 name sprites */
    }
    if (want_w > 64) want_w = 64;
    /* IA8 rows must stay 8-byte aligned for the TMEM row loads — every
     * vanilla name sprite pads width_img to a multiple of 8 (47/48,
     * 30/32, ...). An unpadded stride shears the whole bitmap into
     * diagonal garbage (seen as a 50-stride widen on the 48-wide Mario
     * sprite). Draw width stays exact; only the stride is padded. */
    stride = (want_w + 7) & ~7;
    if (stride > 64) stride = 64;
    old_stride = bms[0].width_img;
    h = bms[0].actualHeight;
    if (stride <= old_stride || sNUIWidens >= (s32)(sizeof sUIWidens / sizeof sUIWidens[0]))
    {
        return bms;
    }
    src = (u8 *)PORT_RESOLVE(bms[0].buf);
    if (src == NULL) return bms;
    clone = (Bitmap *)malloc(sizeof(Bitmap));
    dst = (u8 *)malloc((size_t)(stride * h));
    if (clone == NULL || dst == NULL) return bms;
    memset(dst, 0, (size_t)(stride * h));
    for (y = 0; y < h; y++)
    {
        memcpy(dst + y * stride, src + y * old_stride, (size_t)old_stride);
    }
    clone[0] = bms[0];
    clone[0].width = want_w;
    clone[0].width_img = stride;
    clone[0].buf = portRelocRegisterPointer(dst);
    if (port_ui_widen_find(spr) == NULL)
    {
        sUIWidens[sNUIWidens].spr = spr;
        sUIWidens[sNUIWidens].orig_bitmap = spr->bitmap;
        sUIWidens[sNUIWidens].orig_w = spr->width;
        sNUIWidens++;
    }
    spr->bitmap = portRelocRegisterPointer(clone);
    spr->width = (u16)want_w;
    bufs[0] = dst;
    portMarkSyntheticSprite(spr, clone, 1, bufs);
    port_log("OSBUI: widened name sprite %d -> %d (stride %d)\n",
             (int)old_stride, (int)want_w, (int)stride);
    return clone;
}

static void port_ui_write_canvas(Sprite *spr, const u8 *canvas, s32 canvas_w,
                                 s32 canvas_h, const char *what)
{
    Bitmap *bms;
    s32 b, y, x, yy = 0;
    s32 content_w = 0, avail;
    if (spr == NULL) return;
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    /* content width of the canvas (right-most nonzero column + 1); if it
     * exceeds this fighter's drawable width, nearest-resample to fit —
     * the name sprites are 40/48/64 texels wide depending on character. */
    for (y = 0; y < canvas_h; y++)
    {
        for (x = canvas_w - 1; x >= content_w; x--)
        {
            if (canvas[y * canvas_w + x] != 0)
            {
                content_w = x + 1;
                break;
            }
        }
    }
    avail = (spr->width > 0 && spr->width < canvas_w) ? spr->width + 1 : canvas_w;
    if (avail > bms[0].width_img)
    {
        avail = bms[0].width_img;
    }
    if (content_w > avail)
    {
        /* grow the sprite like vanilla sizes DK's 72-texel name; only if
         * that fails does the nearest-resample below kick in */
        portFixupSpriteBitmapData(spr, bms);
        bms = port_ui_widen(spr, bms, content_w);
        avail = (spr->width > 0 && spr->width < canvas_w) ? spr->width + 1 : canvas_w;
        if (avail > bms[0].width_img)
        {
            avail = bms[0].width_img;
        }
    }
    portFixupSpriteBitmapData(spr, bms);
    for (b = 0; b < spr->nbitmaps; b++)
    {
        u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
        s32 w = bms[b].width_img;
        s32 bpp8 = (spr->bmsiz == G_IM_SIZ_8b);
        s32 row_bytes = bpp8 ? w : (w / 2);
        u8 tmp[256];
        if (buf == NULL || row_bytes > (s32)sizeof(tmp)) continue;
        for (y = 0; y < bms[b].actualHeight; y++)
        {
            s32 cy = yy + y;
            for (x = 0; x < row_bytes; x++) tmp[x] = 0;
            for (x = 0; x < w && x < canvas_w; x++)
            {
                s32 sx = (content_w > avail) ? (x * content_w) / avail : x;
                u8 v = (cy < canvas_h && sx < canvas_w) ? canvas[cy * canvas_w + sx] : 0;
                if (bpp8)
                {
                    tmp[x] = v;
                }
                else if (x & 1)
                {
                    tmp[x / 2] |= (v >> 4);
                }
                else
                {
                    tmp[x / 2] |= (v & 0xF0);
                }
            }
            /* buffer is in the port's linear texel state — rows as-is */
            for (x = 0; x < row_bytes; x++)
            {
                buf[y * row_bytes + x] = tmp[x];
            }
        }
        yy += bms[b].actualHeight;
    }
    port_log("OSBUI: injected %s (canvas %dx%d -> %d bitmaps)\n",
             what, (int)canvas_w, (int)canvas_h, (int)spr->nbitmaps);
}

/* OSBV layout: ['OSBV'][portrait 8640 pre-encoded][name canvas 64x16 IA8]
 * [stock 80 pre-encoded][pal 32][vs canvas 64x12 intensity bytes]
 * [optional emblem canvas 48x48 coverage bytes — series-emblem glyph] */
#define OSBV_NAME_OFF   (4 + 8640)
#define OSBV_STOCK_OFF  (OSBV_NAME_OFF + 64 * 16)
#define OSBV_PAL_OFF    (OSBV_STOCK_OFF + 80)
#define OSBV_VS_OFF     (OSBV_PAL_OFF + 32)
#define OSBV_EMBL_OFF   (OSBV_VS_OFF + 64 * 12)
#define OSBV_EMBL_W     48
#define OSBV_EMBL_H     48

extern void portFixupSpriteBitmapData(void *sprite, void *bitmaps);

void port_ui_snapshot(Sprite *spr);
/* pristine (pre-injection) texels for a sprite, bitmaps concatenated in the
 * converted state — NULL if this sprite was never snapshotted. */
static const u8 *port_ui_snap_texels(Sprite *spr);

/* Write a coverage canvas into a sprite of ANY 4/8-bit intensity geometry,
 * nearest-resampling both axes so the glyph lands exactly where the VANILLA
 * art sat (aspect preserved, centered on the vanilla ink box).
 *
 * Fitting to the sprite's full drawn area is wrong: the art inside these
 * sprites is not centered in its own bitmap. Every CSS series emblem is a
 * 64x48 I4 tile whose ink sits at x[12..25]..[59..63], y[1..8]..[40..44] —
 * centered near x=39.5, not x=32 — because the card places the SObj at a
 * fixed offset (mnPlayersVSMakeNameAndEmblem: pos.x = player*69 + 24) and
 * the padding is baked into the texels. Centering our glyph on the tile
 * therefore pushed it ~7px left (off the left edge of the card) and let it
 * grow taller than any vanilla emblem. So measure the ink box of the
 * pristine texels and fit into THAT — general across every target sprite
 * (CSS card watermark, stage tags, in-match HUD backdrop), no per-target
 * constants. Empty vanilla art falls back to the whole drawn area.
 *
 * Canvas bytes are pure 0-255 coverage, thresholded to a flat
 * full-intensity silhouette with a hard edge — the vanilla emblem look (the
 * engine tints it at draw time). The bitmap data is force-converted to the
 * port's linear texel state first (portFixupSpriteBitmapData is
 * idempotent), and linear bytes are written, so re-running on a later
 * reselect stays correct — writing the DRAM-swizzled state here corrupts on
 * the second pass because the one-time draw fixup will not run again. */
static void port_ui_write_canvas_fit(Sprite *spr, const u8 *canvas, s32 cw,
                                     s32 ch, const char *what)
{
    Bitmap *bms;
    const u8 *snap;
    s32 b, x, y, yy = 0, total_h = 0;
    s32 x0 = cw, x1 = -1, y0 = ch, y1 = -1;
    s32 vx0, vx1, vy0, vy1, vw, vh;
    s32 dw, dh, ow, oh, ox, oy;
    s32 peak_nib = 15;
    f32 s;

    if (spr == NULL) return;
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    if (spr->bmfmt != G_IM_FMT_I && spr->bmfmt != G_IM_FMT_IA)
    {
        port_log("OSBUI: %s target fmt=%d not I/IA — skipped\n", what, spr->bmfmt);
        return;
    }
    if (spr->bmsiz != G_IM_SIZ_4b && spr->bmsiz != G_IM_SIZ_8b)
    {
        port_log("OSBUI: %s target siz=%d not 4/8-bit — skipped\n", what, spr->bmsiz);
        return;
    }
    portFixupSpriteBitmapData(spr, bms);
    /* keep a pristine copy so the vanilla ink box and peak intensity are
     * measured from the ORIGINAL art even when this sprite has already been
     * injected once this session (roster page flips, HUD re-inits) */
    port_ui_snapshot(spr);
    snap = port_ui_snap_texels(spr);
    for (y = 0; y < ch; y++)
    {
        for (x = 0; x < cw; x++)
        {
            if (canvas[y * cw + x] != 0)
            {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    if (x1 < 0)
    {
        x0 = 0; x1 = cw - 1; y0 = 0; y1 = ch - 1;
    }
    for (b = 0; b < spr->nbitmaps; b++) total_h += bms[b].actualHeight;
    dw = (spr->width > 0 && spr->width <= bms[0].width_img) ? spr->width : bms[0].width_img;
    dh = (spr->height > 0 && spr->height <= total_h) ? spr->height : total_h;
    /* Scan the vanilla texels once for two things:
     *   - peak intensity, so the tinted overlay keeps its stock translucency
     *     (I doubles as alpha for I-format sprites; the CSS emblems peak at
     *     9/15, others may differ), and
     *   - the ink box, which is where the art is actually meant to sit
     *     inside the tile (see the note above the function).
     * Reads the pristine snapshot when there is one, so a re-injection
     * measures the original art rather than our own previous write. */
    vx0 = dw; vx1 = -1; vy0 = dh; vy1 = -1;
    {
        s32 peak = 0;
        const u8 *sp = snap;
        for (b = 0; b < spr->nbitmaps; b++)
        {
            const u8 *buf = (const u8 *)PORT_RESOLVE(bms[b].buf);
            s32 w = bms[b].width_img;
            s32 bpp8 = (spr->bmsiz == G_IM_SIZ_8b);
            s32 row_bytes = bpp8 ? w : (w / 2);
            if (sp != NULL) { buf = sp; sp += row_bytes * bms[b].actualHeight; }
            if (buf == NULL) { yy += bms[b].actualHeight; continue; }
            for (y = 0; y < bms[b].actualHeight; y++)
            {
                s32 cy = yy + y;
                for (x = 0; x < w; x++)
                {
                    u8 byte = buf[y * row_bytes + (bpp8 ? x : x / 2)];
                    s32 nib = (bpp8 || !(x & 1)) ? (byte >> 4) : (byte & 0xF);
                    if (nib > peak) peak = nib;
                    if (nib != 0 && x < dw && cy < dh)
                    {
                        if (x < vx0) vx0 = x;
                        if (x > vx1) vx1 = x;
                        if (cy < vy0) vy0 = cy;
                        if (cy > vy1) vy1 = cy;
                    }
                }
            }
            yy += bms[b].actualHeight;
        }
        if (peak > 0) peak_nib = peak;
        yy = 0;
    }
    if (vx1 < 0)
    {
        /* blank target — no ink box to match, use the whole drawn area */
        vx0 = 0; vx1 = dw - 1; vy0 = 0; vy1 = dh - 1;
    }
    vw = vx1 - vx0 + 1;
    vh = vy1 - vy0 + 1;
    s = (f32)vw / (x1 - x0 + 1);
    if ((f32)vh / (y1 - y0 + 1) < s) s = (f32)vh / (y1 - y0 + 1);
    ow = (s32)((x1 - x0 + 1) * s + 0.5F);
    oh = (s32)((y1 - y0 + 1) * s + 0.5F);
    if (ow < 1) ow = 1;
    if (oh < 1) oh = 1;
    if (ow > dw) ow = dw;
    if (oh > dh) oh = dh;
    ox = vx0 + (vw - ow) / 2;
    oy = vy0 + (vh - oh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    if (ox + ow > dw) ox = dw - ow;
    if (oy + oh > dh) oy = dh - oh;
    for (b = 0; b < spr->nbitmaps; b++)
    {
        u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
        s32 w = bms[b].width_img;
        s32 bpp8 = (spr->bmsiz == G_IM_SIZ_8b);
        s32 row_bytes = bpp8 ? w : (w / 2);
        u8 tmp[256];
        if (buf == NULL || row_bytes > (s32)sizeof(tmp)) continue;
        for (y = 0; y < bms[b].actualHeight; y++)
        {
            s32 cy = yy + y;
            for (x = 0; x < row_bytes; x++) tmp[x] = 0;
            for (x = 0; x < w; x++)
            {
                u8 v = 0, t;
                if (x >= ox && x < ox + ow && cy >= oy && cy < oy + oh)
                {
                    s32 sx = x0 + (s32)((x - ox) / s);
                    s32 sy = y0 + (s32)((cy - oy) / s);
                    if (sx > x1) sx = x1;
                    if (sy > y1) sy = y1;
                    v = canvas[sy * cw + sx];
                }
                /* scale coverage to the target's vanilla peak intensity
                 * (I doubles as alpha for I-format sprites; the CSS
                 * emblems peak at 9/15 for the translucent watermark) and
                 * mirror the nibble into both halves so the same byte
                 * serves 4-bit (high nibble) and 8-bit. */
                t = (u8)((v * peak_nib + 127) / 255);
                t = (u8)((t << 4) | t);
                if (bpp8)
                {
                    tmp[x] = t;
                }
                else if (x & 1)
                {
                    tmp[x / 2] |= (t >> 4);
                }
                else
                {
                    tmp[x / 2] |= (t & 0xF0);
                }
            }
            /* buffer is in the port's linear texel state — write rows as-is */
            for (x = 0; x < row_bytes; x++)
            {
                buf[y * row_bytes + x] = tmp[x];
            }
        }
        yy += bms[b].actualHeight;
    }
    port_log("OSBUI: injected %s (canvas %dx%d -> %dx%d at %d,%d; vanilla ink "
             "%dx%d at %d,%d in %dx%d)\n",
             what, (int)cw, (int)ch, (int)ow, (int)oh, (int)ox, (int)oy,
             (int)vw, (int)vh, (int)vx0, (int)vy0, (int)dw, (int)dh);
}

s32 port_ui_target_fkind(void);

/* Read the OSBV emblem canvas (if the file carries one) and write it into
 * the given emblem sprite. Shared by the menu hook and the in-match HUD
 * (damage-backdrop) injection. */
static void port_ui_apply_emblem(Sprite *spr, const char *ui, const char *what)
{
    static u8 canvas[OSBV_EMBL_W * OSBV_EMBL_H];
    FILE *f;
    char m[4];

    if (spr == NULL || ui == NULL)
    {
        return;
    }
    f = fopen(ui, "rb");
    if (f == NULL)
    {
        return;
    }
    if (fread(m, 1, 4, f) == 4 && m[0] == 'O' && m[3] == 'V' &&
        fseek(f, OSBV_EMBL_OFF, SEEK_SET) == 0 &&
        fread(canvas, 1, sizeof canvas, f) == sizeof canvas)
    {
        port_ui_write_canvas_fit(spr, canvas, OSBV_EMBL_W, OSBV_EMBL_H, what);
    }
    fclose(f);
}

/* -------------------------------------------------------------------- */
/* Pristine-texel snapshots: roster page flips repaint the CSS tiles, so */
/* the first touch of each sprite stores its vanilla bytes and unbinding */
/* a tile restores them. Snapshots are taken in the same state writes    */
/* happen in (linear for 4/8-bit after the forced fixup; 32bpp raw).     */
/* -------------------------------------------------------------------- */
#define UI_SNAP_MAX 48
typedef struct { u8 *key; u32 total; u8 *data; } UISnap;
static UISnap sUISnaps[UI_SNAP_MAX];
static s32 sNUISnaps = 0;
static s32 sUISnapNext = 0;

static u32 port_ui_sprite_bytes(Sprite *spr, Bitmap *bms, s32 b)
{
    s32 bpt = (spr->bmsiz == G_IM_SIZ_16b) ? 16 : (spr->bmsiz == G_IM_SIZ_8b) ? 8 :
              (spr->bmsiz == G_IM_SIZ_32b) ? 32 : 4;
    return (u32)((bms[b].width_img * bms[b].actualHeight * bpt) / 8);
}

static void port_ui_fix_for_snap(Sprite *spr, Bitmap *bms)
{
    /* the one-time draw fixup converts EVERY size (32bpp gets its BSWAP32
     * restore too) — force it so snapshots/writes always see the
     * converted state */
    portFixupSpriteBitmapData(spr, bms);
}

static UISnap *port_ui_snap_find(u8 *key)
{
    s32 i;
    for (i = 0; i < sNUISnaps; i++)
    {
        if (sUISnaps[i].key == key) return &sUISnaps[i];
    }
    return NULL;
}

/* Store the sprite's current texels if not seen before. Call BEFORE the
 * first injection write of a scene. */
void port_ui_snapshot(Sprite *spr)
{
    Bitmap *bms;
    u8 *key;
    u32 total = 0;
    s32 b;
    UISnap *sn;
    u8 *dst;

    if (spr == NULL) return;
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    port_ui_fix_for_snap(spr, bms);
    key = (u8 *)PORT_RESOLVE(bms[0].buf);
    if (key == NULL || port_ui_snap_find(key) != NULL) return;
    for (b = 0; b < spr->nbitmaps; b++) total += port_ui_sprite_bytes(spr, bms, b);
    if (total == 0 || total > 64 * 1024) return;
    if (sNUISnaps < UI_SNAP_MAX)
    {
        sn = &sUISnaps[sNUISnaps++];
    }
    else
    {
        sn = &sUISnaps[sUISnapNext];
        sUISnapNext = (sUISnapNext + 1) % UI_SNAP_MAX;
        free(sn->data);
    }
    sn->key = key;
    sn->total = total;
    sn->data = (u8 *)malloc(total);
    dst = sn->data;
    for (b = 0; b < spr->nbitmaps; b++)
    {
        u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
        u32 n = port_ui_sprite_bytes(spr, bms, b);
        if (buf != NULL) memcpy(dst, buf, n);
        dst += n;
    }
}

/* Pristine texels for a sprite (bitmaps concatenated, converted state), or
 * NULL if it was never snapshotted. Lets a re-injection measure the ORIGINAL
 * art's ink box and peak intensity instead of our own previous write. */
static const u8 *port_ui_snap_texels(Sprite *spr)
{
    Bitmap *bms;
    UISnap *sn;

    if (spr == NULL) return NULL;
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return NULL;
    sn = port_ui_snap_find((u8 *)PORT_RESOLVE(bms[0].buf));
    return (sn != NULL) ? sn->data : NULL;
}

/* Put the pristine texels back (tile unbound on this roster page). */
static void port_ui_unwiden(Sprite *spr);
void port_ui_restore(Sprite *spr)
{
    Bitmap *bms;
    UISnap *sn;
    u8 *src;
    s32 b;

    if (spr == NULL) return;
    port_ui_unwiden(spr);       /* revert any injected-name widening first */
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    port_ui_fix_for_snap(spr, bms);
    sn = port_ui_snap_find((u8 *)PORT_RESOLVE(bms[0].buf));
    if (sn == NULL) return;
    src = sn->data;
    for (b = 0; b < spr->nbitmaps; b++)
    {
        u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
        u32 n = port_ui_sprite_bytes(spr, bms, b);
        if (buf != NULL) memcpy(buf, src, n);
        src += n;
    }
}

/* Give an SObj a PRIVATE copy of its sprite's bitmaps + texels. The CSS
 * card name/emblem SObjs otherwise reference the shared per-fkind sprite
 * data, which the roster page repaints rewrite — a placed card would
 * morph into whatever the current page shows on that tile. Cloning at
 * selection time pins the card to the pick. Buffers are copied in the
 * converted state and registered as synthetic so the one-time draw
 * fixups skip them. */
extern unsigned int portRelocRegisterPointer(void *ptr);
extern void portMarkSyntheticSprite(void *sprite, void *bitmaps, unsigned int nbitmaps, void **bufs);

void port_ui_privatize_sprite(Sprite *spr)
{
    Bitmap *bms;
    Bitmap *clone;
    void *bufs[8];
    s32 b, n;

    if (spr == NULL)
    {
        return;
    }
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL)
    {
        return;
    }
    n = spr->nbitmaps;
    if (n <= 0 || n > 8)
    {
        return;
    }
    /* source is fully converted already (snapshot/write machinery forces
     * the fixups before any roster repaint) */
    portFixupSpriteBitmapData(spr, bms);
    clone = (Bitmap *)malloc(sizeof(Bitmap) * n);
    if (clone == NULL)
    {
        return;
    }
    for (b = 0; b < n; b++)
    {
        u8 *src = (u8 *)PORT_RESOLVE(bms[b].buf);
        u32 nbytes = port_ui_sprite_bytes(spr, bms, b);
        u8 *dst;
        clone[b] = bms[b];
        dst = (u8 *)malloc(nbytes);
        if (src != NULL && dst != NULL)
        {
            memcpy(dst, src, nbytes);
        }
        clone[b].buf = portRelocRegisterPointer(dst);
        bufs[b] = dst;
    }
    spr->bitmap = portRelocRegisterPointer(clone);
    portMarkSyntheticSprite(spr, clone, (unsigned int)n, bufs);
}

/* Series-emblem hook — called wherever a menu screen makes an SObj from
 * an llFTEmblemSprites entry (CSS card watermark, stage-select tags).
 * The emblem file data is shared per SERIES, so replacing the injection
 * target's emblem also restyles series-mates on the same screen (e.g.
 * Luigi's card when Mario is the target) — acceptable for the harness. */
void port_ui_emblem_hook(Sprite *emblem, s32 fkind)
{
    const char *dump = getenv("SSB64_DUMP_SPRITES");
    const char *ui = port_ui_path_for_fkind(fkind);

    if (emblem == NULL)
    {
        return;
    }
    if (dump != NULL)
    {
        char nm[32];
        snprintf(nm, sizeof nm, "emblem_fk%d", (int)fkind);
        port_ui_dump_sprite(dump, nm, emblem);
    }
    port_ui_snapshot(emblem);
    if (ui == NULL)
    {
        port_ui_restore(emblem);
        return;
    }
    port_ui_apply_emblem(emblem, ui, "emblem");
}

/* Called from the VS character-select screen right after its reloc
 * files load. portrait/name are Mario's (fkind of the injection target
 * is always 0 for now — the gate lives in the caller). */
/* Which fighter kind the injection replaces (drives UI slot choice). */
s32 port_ui_target_fkind(void)
{
    const char *e = getenv("SSB64_INJECT_FKIND");
    return (e != NULL) ? atoi(e) : 0;
}

/* ------------------------------------------------------------------ */
/* Injected announcer voice (SSB64_INJECT_VOICE)                       */
/* ------------------------------------------------------------------ */

#include <gm/gmsound.h>
#include "audio/voice_inject.h"

/* Announcer name FGM id per vanilla fkind — mirrors the announce_names
 * tables in mnplayersvs.c / mnvsresults.c / mnplayers1p*.c. */
static u16 port_voice_announce_name_fgm(s32 fkind)
{
    static const u16 names[] =
    {
        nSYAudioVoiceAnnounceMario,
        nSYAudioVoiceAnnounceFox,
        nSYAudioVoiceAnnounceDonkey,
        nSYAudioVoiceAnnounceSamus,
        nSYAudioVoiceAnnounceLuigi,
        nSYAudioVoiceAnnounceLink,
        nSYAudioVoiceAnnounceYoshi,
        nSYAudioVoiceAnnounceCaptain,
        nSYAudioVoiceAnnounceKirby,
        nSYAudioVoiceAnnouncePikachu,
        nSYAudioVoiceAnnouncePurin,
        nSYAudioVoiceAnnounceNess
    };
    return ((u32)fkind < ARRAY_COUNT(names)) ? names[fkind] : 0xFFFF;
}

/* Called from func_800269C0_275C0 (the universal FGM-play entry) before it
 * touches the FGM engine. Returns nonzero when the requested id is the
 * injection target's announcer name — the WAV overlay plays instead and
 * the FGM call is suppressed (callers already tolerate a NULL sfx handle:
 * the harness path takes it for synth fkinds too). Any other vanilla
 * announcer name cuts a still-running overlay, matching how the game stops
 * the previous name clip when a new one starts. */
s32 port_voice_announce_filter(u16 id)
{
    s32 i;

    /* no Available() gate: the roster provides per-tile clips without the
     * legacy SSB64_INJECT_VOICE env being set */
    for (i = 0; i < 12; i++)
    {
        if (id == port_voice_announce_name_fgm(i))
        {
            const char *path = port_voice_path_for_fkind(i);
            if (path != NULL)
            {
                portVoiceInjectPlayPath(path);
                return 1;
            }
            /* a vanilla name supersedes a still-running injected clip */
            portVoiceInjectStop();
            return 0;
        }
    }
    return 0;
}

/* VS-results plays the crowd cheer a fixed 60 tics after the winner's name.
 * Extra tics to wait so a longer generated name can finish first. */
s32 port_voice_results_extra_wait_tics(void)
{
    s32 dur = portVoiceInjectDurationTics();   /* 0 when no clip active */
    return (dur > 60) ? dur - 60 : 0;
}

/* Overwrite a sprite's texel bytes with pre-encoded data (already in
 * the file's blanket-swapped + TMEM-swizzled DRAM state, produced by
 * pipeline/gen_ui_assets.py). Struct fixups are forced first so sizes
 * read true; the texel fixup that runs at first draw then applies to
 * our bytes exactly as it would to the originals. */
/* File payloads are pre-encoded to the RAW post-load state (blanket u32
 * swap + TMEM swizzle). The target buffer is force-converted to the
 * port's draw state first, and the same conversion is applied to the
 * file bytes (u32 byteswap; odd-row 8-byte-group half swap for sub-32bpp)
 * — so writes are idempotent and roster-page repaints stay correct. */
static void port_ui_write_sprite(Sprite *spr, FILE *f, const char *what)
{
    Bitmap *bms;
    s32 b;
    if (spr == NULL) return;
    portFixupSprite(spr);
    bms = (Bitmap *)PORT_RESOLVE(spr->bitmap);
    if (bms == NULL) return;
    portFixupBitmapArray(bms, spr->nbitmaps);
    portFixupSpriteBitmapData(spr, bms);
    for (b = 0; b < spr->nbitmaps; b++)
    {
        u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
        s32 bpt = (spr->bmsiz == G_IM_SIZ_16b) ? 16 : (spr->bmsiz == G_IM_SIZ_8b) ? 8 :
                  (spr->bmsiz == G_IM_SIZ_32b) ? 32 : 4;
        s32 nbytes = (bms[b].width_img * bms[b].actualHeight * bpt) / 8;
        s32 row_bytes = (bms[b].width_img * bpt) / 8;
        s32 x, y;
        static u8 tmp[16384];
        if (buf == NULL || nbytes > (s32)sizeof(tmp) ||
            fread(tmp, 1, nbytes, f) != (size_t)nbytes)
        {
            port_log("OSBUI: short read injecting %s bitmap %d\n", what, (int)b);
            return;
        }
        /* undo the blanket u32 byte reversal */
        for (x = 0; x + 3 < nbytes; x += 4)
        {
            u8 t0 = tmp[x], t1 = tmp[x + 1];
            tmp[x] = tmp[x + 3]; tmp[x + 1] = tmp[x + 2];
            tmp[x + 2] = t1; tmp[x + 3] = t0;
        }
        /* undo the odd-row TMEM line swizzle (16-byte groups for 32bpp,
         * 8-byte groups otherwise — matches portFixupSpriteBitmapData) */
        if (row_bytes > 0)
        {
            s32 grp = (spr->bmsiz == G_IM_SIZ_32b) ? 16 : 8;
            s32 half = grp / 2;
            for (y = 1; y * row_bytes < nbytes; y += 2)
            {
                u8 *row = tmp + y * row_bytes;
                for (x = 0; x + grp - 1 < row_bytes; x += grp)
                {
                    s32 k;
                    for (k = 0; k < half; k++)
                    {
                        u8 t = row[x + k];
                        row[x + k] = row[x + half + k];
                        row[x + half + k] = t;
                    }
                }
            }
        }
        memcpy(buf, tmp, nbytes);
    }
    port_log("OSBUI: injected %s (%d bitmaps)\n", what, (int)spr->nbitmaps);
}

void port_ui_css_hook(Sprite *portrait, Sprite *name_text, Sprite *fire_bg, s32 fkind)
{
    const char *dump = getenv("SSB64_DUMP_SPRITES");
    const char *ui = port_ui_path_for_fkind(fkind);
    if (dump != NULL && fkind == port_ui_target_fkind())
    {
        port_ui_dump_sprite(dump, "css_portrait", portrait);
        port_ui_dump_sprite(dump, "css_name", name_text);
        port_ui_dump_sprite(dump, "css_firebg", fire_bg);
    }
    if (dump != NULL)
    {
        /* full-roster name dump: pipeline glyph-atlas source material */
        char nm[32];
        snprintf(nm, sizeof nm, "css_name_fk%d", (int)fkind);
        port_ui_dump_sprite(dump, nm, name_text);
    }
    port_ui_snapshot(portrait);
    port_ui_snapshot(name_text);
    if (ui == NULL)
    {
        port_ui_restore(portrait);
        port_ui_restore(name_text);
    }
    if (ui != NULL)
    {
        FILE *f = fopen(ui, "rb");
        char magic[4];
        if (f == NULL)
        {
            port_log("OSBUI: cannot open %s\n", ui);
            return;
        }
        fread(magic, 1, 4, f);
        if (magic[0] != 'O' || magic[1] != 'S' || magic[2] != 'B' ||
            (magic[3] != 'U' && magic[3] != 'V'))
        {
            port_log("OSBUI: bad magic in %s\n", ui);
            fclose(f);
            return;
        }
        port_ui_write_sprite(portrait, f, "css_portrait");
        if (magic[3] == 'V')
        {
            static u8 canvas[64 * 16];
            fseek(f, OSBV_NAME_OFF, SEEK_SET);
            if (fread(canvas, 1, sizeof canvas, f) == sizeof canvas)
            {
                port_ui_write_canvas(name_text, canvas, 64, 16, "css_name");
            }
        }
        else
        {
            port_ui_write_sprite(name_text, f, "css_name");
        }
        fclose(f);
        if (dump != NULL && fkind == port_ui_target_fkind())
        {
            port_ui_dump_sprite(dump, "css_name_post", name_text);
            port_ui_dump_sprite(dump, "css_portrait_post", portrait);
        }
    }
}

/* VS-splash name (CharacterNames file): dumped as vs_name, injected from
 * the OSBU section after the stock palette. */
void port_ui_vs_hook(Sprite *name_sprite, s32 fkind)
{
    const char *dump = getenv("SSB64_DUMP_SPRITES");
    const char *ui = port_ui_path_for_fkind(fkind);
    if (dump != NULL)
    {
        port_ui_dump_sprite(dump, "vs_name", name_sprite);
    }
    if (ui != NULL)
    {
        FILE *f = fopen(ui, "rb");
        if (f != NULL)
        {
            char m[4];
            fread(m, 1, 4, f);
            if (m[0] == 'O' && m[3] == 'V')
            {
                static u8 canvas[64 * 12];
                fseek(f, OSBV_VS_OFF, SEEK_SET);
                if (fread(canvas, 1, sizeof canvas, f) == sizeof canvas)
                {
                    port_ui_write_canvas(name_sprite, canvas, 64, 12, "vs_name");
                }
            }
            else if (m[0] == 'O' && m[3] == 'U')
            {
                /* legacy: [magic][portrait 8640][name 768][stock 80][pal 32][vs name] */
                fseek(f, 4 + 8640 + 768 + 80 + 32, SEEK_SET);
                port_ui_write_sprite(name_sprite, f, "vs_name");
            }
            if (dump != NULL)
            {
                port_ui_dump_sprite(dump, "vs_name_post", name_sprite);
            }
            fclose(f);
        }
    }
}

void port_inject_bundle(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    const char *path;
    s32 from_single = 0;
    FILE *f;
    char magic[4];
    u32 nparts, p;
    u32 replaced = 0;

    if (fp == NULL)
    {
        return;
    }
    {
        s32 cidx = -1;
        PortChar *c = port_char_for_player((s32)fp->player, (s32)fp->fkind, &cidx);
        sInjectCharIdx = (c != NULL) ? cidx : -1;
        if (c != NULL)
        {
            /* registry character: only when the skeleton its bundle targets
             * (base fighter when declared, else home tile) matches the
             * fighter actually spawning (transient CSS re-makes and preset
             * players on foreign boots must not cross-skin). */
            s32 eff = (c->base >= 0) ? c->base : c->fkind;
            path = (eff == (s32)fp->fkind && c->bundle[0] != '\0') ? c->bundle : NULL;
        }
        else
        {
            path = port_inject_bundle_path((s32)fp->fkind, &from_single);
        }
    }
    if (path == NULL)
    {
        /* uninjected spawn: disown any mesh slot still pointing at this
         * GObj — the pool reuses addresses, and a stale owner match (same
         * gobj, same fkind after re-picking vanilla on the injected
         * character's home tile) would keep blanking the vanilla mesh */
        OSB5State *o = osb5_slot((s32)fp->player);
        if (o != NULL && o->owner == fighter_gobj)
        {
            o->owner = NULL;
        }
        return;
    }
    /* Optional per-player gate (single-target mode only): lets a vanilla
     * twin of the same fkind fight alongside the injected one for A/B
     * comparison. Roster (SET) entries always apply to every player of
     * their fkind. */
    if (from_single)
    {
        const char *pl_env = getenv("SSB64_INJECT_PLAYER");
        if (pl_env != NULL && (int)fp->player != atoi(pl_env))
        {
            return;
        }
    }

    {
        const char *dump = getenv("SSB64_DUMP_SPRITES");
        const char *ui = port_ui_path_for_player((s32)fp->player, (s32)fp->fkind);
        if ((dump != NULL || ui != NULL) && fp->attr != NULL && fp->attr->sprites != NULL)
        {
            FTSprites *_spr = (FTSprites *)PORT_RESOLVE(fp->attr->sprites);
            Sprite *st = (Sprite *)PORT_RESOLVE(_spr->stock_sprite);
            if (st != NULL && dump != NULL)
            {
                port_ui_dump_sprite(dump, "stock_icon", st);
            }
            /* in-match HUD damage-backdrop emblem (per-fighter sprite in
             * the fighter's own data, tinted per player color) */
            {
                Sprite *hud_emb = (Sprite *)PORT_RESOLVE(_spr->emblem);
                if (hud_emb != NULL && dump != NULL)
                {
                    port_ui_dump_sprite(dump, "hud_emblem", hud_emb);
                }
                if (hud_emb != NULL && ui != NULL)
                {
                    port_ui_apply_emblem(hud_emb, ui, "hud_emblem");
                }
            }
            /* stock icon pixels + costume-0 palette live at the end of the
             * OSBU file: [magic][portrait][name][stock ci4][16x u16 pal] */
            if (st != NULL && ui != NULL)
            {
                FILE *uf = fopen(ui, "rb");
                if (uf != NULL)
                {
                    char m[4];
                    fread(m, 1, 4, uf);
                    if (m[0] == 'O' && (m[3] == 'U' || m[3] == 'V'))
                    {
                        if (m[3] == 'V') fseek(uf, OSBV_STOCK_OFF, SEEK_SET);
                        else fseek(uf, -(80 + 32), SEEK_END);
                        port_ui_write_sprite(st, uf, "stock_icon");
                        {
                            u32 *_luts = (u32 *)PORT_RESOLVE(_spr->stock_luts);
                            u8 *lut0 = (_luts != NULL) ? (u8 *)PORT_RESOLVE(_luts[0]) : NULL;
                            if (lut0 != NULL && fread(lut0, 1, 32, uf) == 32)
                            {
                                port_log("OSBUI: injected stock palette (costume 0)\n");
                            }
                        }
                    }
                    fclose(uf);
                }
            }
        }
    }

    f = fopen(path, "rb");
    if (f == NULL)
    {
        port_log("OSB: cannot open %s\n", path);
        return;
    }
    fread(magic, 1, 4, f);
    if (magic[0] == 'O' && magic[1] == 'S' && magic[2] == 'B' && magic[3] == '5')
    {
        osb5_load(fp, f);
        fclose(f);
        return;
    }
    if (magic[0] != 'O' || magic[1] != 'S' || magic[2] != 'B' ||
        (magic[3] != '2' && magic[3] != '3' && magic[3] != '4'))
    {
        port_log("OSB: bad magic in %s (want OSB2/OSB3/OSB4)\n", path);
        fclose(f);
        return;
    }
    fread(&nparts, 4, 1, f);

    if (magic[3] == '3' || magic[3] == '4')
    {
        u32 twh[2];
        u8 *tex;
        fread(twh, 4, 2, f);
        tex = (u8 *)malloc(twh[0] * twh[1] * 2);
        if (tex == NULL)
        {
            fclose(f);
            return;
        }
        fread(tex, 2, twh[0] * twh[1], f);
        port_log("OSB3: injecting %s (%d parts, %dx%d atlas) into fkind=%d\n",
                 path, (int)nparts, (int)twh[0], (int)twh[1], (int)fp->fkind);

        for (p = 0; p < nparts; p++)
        {
            u32 hdr[2];
            Gfx *dl;

            fread(hdr, 4, 2, f);
            dl = osbBuildPartDL3(f, hdr[1], tex, twh[0], twh[1], magic[3] == '4');
            if (dl != NULL && hdr[0] < FTPARTS_JOINT_NUM_MAX && fp->joints[hdr[0]] != NULL)
            {
                fp->joints[hdr[0]]->dl = dl;
                replaced++;
            }
        }
        fclose(f);
        port_log("OSB3: replaced %d joint DLs\n", (int)replaced);
        return;
    }

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
