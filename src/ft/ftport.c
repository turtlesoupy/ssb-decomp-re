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
    OSB5Vert *src;          /* source verts, spawn-world space */
    f32 (*bind_local)[4][3];/* per vert, per influence: joint-local coords */
    f32 (*bind_nrm)[4][3];  /* per vert, per influence: joint-local normal */
    Vtx *vtx;               /* live Vtx array the DL renders */
    GObj *owner;
    /* GObjs are pool-allocated: after the owner despawns (match end, CSS
     * chip move) the next fighter can reuse the same address, and a bare
     * pointer match would blank a VANILLA fighter's joints (the broken
     * CSS previews). Ownership therefore also requires the fighter kind
     * to match; every spawn of the injected kind re-attaches and
     * refreshes both. */
    s32 owner_fkind;
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

static void osb5_blank_joint(FTStruct *fp, s32 jid)
{
    DObj *j = fp->joints[jid];
    FTParts *parts;
    if (j == NULL)
    {
        return;
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

void port_osb5_skin_update(GObj *fighter_gobj)
{
    FTStruct *fp;
    OSB5State *o;
    f32 jo[32][3], jm[32][3][3];
    f32 t0o[3], t0m[3][3], t0inv[3][3];
    s32 k, i;

    if (getenv("SSB64_NO_SKIN") != NULL) return;
    fp = ftGetStruct(fighter_gobj);
    if (fp == NULL) return;
    o = osb5_slot((s32)fp->player);
    if (o == NULL || o->vtx == NULL || o->owner != fighter_gobj)
    {
        return;
    }
    if ((s32)fp->fkind != o->owner_fkind) return;

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
            osb5_blank_joint(fp, jid);
        }
    }
    /* keep the root on the plain-DL path (modelpart swaps copy flags) */
    if (fp->joints[0] != NULL)
    {
        FTParts *rparts = (FTParts *)fp->joints[0]->user_data.p;
        if (rparts != NULL && (rparts->flags & 0xF) != 0)
        {
            rparts->flags &= ~0xF;
        }
    }
    }
    for (k = 0; k < o->njoints; k++)
    {
        s32 jid = (s32)o->joint_ids[k];
        if (fp->joints[jid] == NULL) return;
        {
            const char *upto = getenv("SSB64_SKIN_UPTO");
            if (upto != NULL && k >= atoi(upto)) continue;
        }
        osb5_joint_frame(fp, jid, jo[k], jm[k]);
    }
    if (fp->joints[0] == NULL) return;
    if (getenv("SSB64_NO_ROOTFRAME") != NULL) return;
    osb5_joint_frame(fp, 0, t0o, t0m);
    osb5_inv3(t0m, t0inv);

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
    }

    if (getenv("SSB64_SKIN_FRAMES_ONLY") != NULL) return;
    for (i = 0; i < o->nverts; i++)
    {
        OSB5Vert *v = &o->src[i];
        f32 acc[3] = {0.0f, 0.0f, 0.0f};
        f32 nacc[3] = {0.0f, 0.0f, 0.0f};
        f32 wl[3], nw[3], nl[3], nlen, wsum = 0.0f;
        s32 t;
        for (t = 0; t < 4; t++)
        {
            f32 w = (f32)v->w[t] / 255.0f;
            f32 *bl, *bn;
            s32 kk = v->j[t];
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
    fread(o->joint_ids, 4, njoints, f);

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
                    Vtx *wv = (Vtx *)malloc(sizeof(Vtx) * count);
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
                    osb5_blank_joint(fp, jj);
                    break;
                }
            }
        }
        if (getenv("SSB64_NO_ROOTDL") == NULL)
        {
            /* the mesh must draw via the plain-DL path */
            FTParts *rparts = (FTParts *)fp->joints[0]->user_data.p;
            if (rparts != NULL && (rparts->flags & 0xF) != 0)
            {
                rparts->flags &= ~0xF;
            }
            fp->joints[0]->dl = dl;
        }
    }
    o->owner = fp->fighter_gobj;
    o->owner_fkind = (s32)fp->fkind;
    port_log("OSB5: skinned mesh attached (%u verts, %u tris, %u joints) player=%d fkind=%d gobj=%p\n",
             nverts, ntris, njoints, (int)fp->player, (int)fp->fkind, (void *)fp->fighter_gobj);
    port_osb5_skin_update(fp->fighter_gobj);
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

/* Write a coverage canvas into a sprite of ANY 4/8-bit intensity geometry,
 * nearest-resampling both axes so the glyph's content bbox fills the
 * sprite's drawn area (aspect preserved, centered). Canvas bytes are pure
 * 0-255 coverage, thresholded to a flat full-intensity silhouette with a
 * hard edge — the vanilla emblem look (the engine tints it at draw time).
 * The bitmap data is force-converted to the port's linear texel state
 * first (portFixupSpriteBitmapData is idempotent), and linear bytes are
 * written, so re-running on a later reselect stays correct — writing the
 * DRAM-swizzled state here corrupts on the second pass because the
 * one-time draw fixup will not run again. */
static void port_ui_write_canvas_fit(Sprite *spr, const u8 *canvas, s32 cw,
                                     s32 ch, const char *what)
{
    Bitmap *bms;
    s32 b, x, y, yy = 0, total_h = 0;
    s32 x0 = cw, x1 = -1, y0 = ch, y1 = -1;
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
    /* match the vanilla art's peak intensity so the tinted overlay keeps
     * its stock translucency (I doubles as alpha for I-format sprites).
     * Scanning the current texels self-calibrates per target — the CSS
     * emblems peak at 9/15, others may differ — and is stable across
     * rewrites since our own output peaks at the same value. */
    {
        s32 peak = 0;
        for (b = 0; b < spr->nbitmaps; b++)
        {
            u8 *buf = (u8 *)PORT_RESOLVE(bms[b].buf);
            s32 n = bms[b].width_img * bms[b].actualHeight;
            s32 i;
            if (buf == NULL) continue;
            if (spr->bmsiz == G_IM_SIZ_4b) n /= 2;
            for (i = 0; i < n; i++)
            {
                if ((buf[i] >> 4) > peak) peak = buf[i] >> 4;
                if (spr->bmsiz == G_IM_SIZ_4b && (buf[i] & 0xF) > peak) peak = buf[i] & 0xF;
            }
        }
        if (peak > 0) peak_nib = peak;
    }
    for (b = 0; b < spr->nbitmaps; b++) total_h += bms[b].actualHeight;
    dw = (spr->width > 0 && spr->width <= bms[0].width_img) ? spr->width : bms[0].width_img;
    dh = (spr->height > 0 && spr->height <= total_h) ? spr->height : total_h;
    s = (f32)dw / (x1 - x0 + 1);
    if ((f32)dh / (y1 - y0 + 1) < s) s = (f32)dh / (y1 - y0 + 1);
    ow = (s32)((x1 - x0 + 1) * s + 0.5F);
    oh = (s32)((y1 - y0 + 1) * s + 0.5F);
    if (ow < 1) ow = 1;
    if (oh < 1) oh = 1;
    ox = (dw - ow) / 2;
    oy = (dh - oh) / 2;
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
    port_log("OSBUI: injected %s (canvas %dx%d -> %dx%d in %dx%d)\n",
             what, (int)cw, (int)ch, (int)ow, (int)oh, (int)dw, (int)dh);
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

/* Put the pristine texels back (tile unbound on this roster page). */
void port_ui_restore(Sprite *spr)
{
    Bitmap *bms;
    UISnap *sn;
    u8 *src;
    s32 b;

    if (spr == NULL) return;
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
        PortChar *c = port_char_for_player((s32)fp->player, (s32)fp->fkind, NULL);
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
