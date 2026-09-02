/*
 * scvsintro.c — OpenSmash "A VS B" card for VS mode (PORT only).
 *
 * Vanilla VS mode goes stage select -> battle with no matchup screen; the
 * only "[player] VS [opponent]" card lives in 1P Game (sc1pintro.c). With a
 * roster of a thousand generated fighters nobody knows who they are about
 * to fight, so scmanager runs this scene right before scVSBattleStartScene.
 *
 * This is a FORK of sc1pintro.c, not a mode of it: the 1P intro stays
 * byte-identical in behaviour. It reuses the 1P intro's assets (sky,
 * banners, VS decal, CharacterNames sprites) and its fighter pose, but lays
 * out 2-4 fighters from the VS transfer state side by side, each in its own
 * camera slice, with the roster's VS-name sprite and announcer clip per
 * bound player: "A ... versus ... B ... versus ... C".
 *
 * Off with SSB64_VS_INTRO=0, and always off under replay record/play so
 * tick-exact harness runs are unaffected.
 */
#ifdef PORT

#include <ft/fighter.h>
#include <sc/scene.h>
#include <sys/video.h>
#include <sys/rdp.h>
#include <reloc_data.h>
#include <sys/audio.h>
#include <sys/scheduler.h>
extern char *getenv(const char *);
#include <string.h>
#include <math.h>

extern void func_800266A0_272A0();
extern void* func_800269C0_275C0(u16);
extern u32 sySchedulerGetTicCount();

/* ftport.c */
extern s32 port_voice_announce_player(s32 player, s32 fkind);
extern const char *port_roster_player_display(s32 player);
/* port/audio/voice_inject */
extern void portVoiceInjectStop(void);
/* n_env.c: drop fighter voice lines (victory poses) while the card runs */
extern s32 gPortFighterVoiceMute;
/* sc1pintro.c — per-fighter look-at heights of the 1P card cameras */
extern CObjDesc* sc1PIntroGetFighterCObjDesc(CObjDesc *cobj_desc, s32 fkind, s32 cobj_id);

#define SCVSINTRO_MAX_FIGHTERS 4
#define SCVSINTRO_VIEW_L 10
#define SCVSINTRO_VIEW_R 310
#define SCVSINTRO_VIEW_T 10
#define SCVSINTRO_VIEW_B 230
#define SCVSINTRO_NAME_Y 196.0F
#define SCVSINTRO_VS_W 50               /* gap holding the VS decal */

/* Fighter-card layout, derived from where the 1P card cameras look (the
 * fighter's mid-height per kind); the 1P camera distances themselves are
 * unusable without their per-kind fovy animations, so the dolly is ours. */
#define SCVSINTRO_DIST_PER_ATY 14.0F

typedef struct SCVSIntroSlot
{
    s32 player;
    s32 fkind;
    s32 costume;
    s32 shade;
    s32 pkind;
    s32 x0, x1;             /* viewport slice */
    f32 half_w;             /* world half-width visible at the fighter */
    GObj *fighter_gobj;
    GObj *camera_gobj;
} SCVSIntroSlot;

static u32 dSCVSIntroFileIDs[] = { llSC1PIntroFileID, llIFCommonAnnounceCommonFileID };
static void *sSCVSIntroFiles[ARRAY_COUNT(dSCVSIntroFileIDs)];

static Lights1 dSCVSIntroLights1 = gdSPDefLights1(0x20, 0x20, 0x20, 0xFF, 0xFF, 0xFF, 0x3C, 0x3C, 0x3C);

static Gfx dSCVSIntroDisplayList[] =
{
    gsSPSetGeometryMode(G_LIGHTING),
    gsSPSetLights1(dSCVSIntroLights1),
    gsSPEndDisplayList()
};

static SCVSIntroSlot sSCVSIntroSlots[SCVSINTRO_MAX_FIGHTERS];
static s32 sSCVSIntroSlotsNum;
static void *sSCVSIntroFigatreeHeaps[SCVSINTRO_MAX_FIGHTERS];
static LBFileNode sSCVSIntroForceStatusBuffer[7];
static LBFileNode sSCVSIntroStatusBuffer[100];
static s32 sSCVSIntroTotalTics;
static s32 sSCVSIntroAnnounceStep;
static s32 sSCVSIntroAnnounceNextTic;
static s32 sSCVSIntroFinishTic;

/* ------------------------------------------------------------------ */
/*  Gate                                                               */
/* ------------------------------------------------------------------ */

sb32 port_vs_intro_enabled(void)
{
    const char *e = getenv("SSB64_VS_INTRO");
    if (e != NULL && e[0] == '0')
    {
        return FALSE;
    }
    if (getenv("SSB64_REPLAY_PLAY") != NULL || getenv("SSB64_REPLAY_RECORD") != NULL)
    {
        return FALSE;
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  Slots                                                              */
/* ------------------------------------------------------------------ */

static void scVSIntroInitVars(void)
{
    s32 i, n = 0, w;

    sSCVSIntroSlotsNum = 0;
    for (i = 0; i < GMCOMMON_PLAYERS_MAX && n < SCVSINTRO_MAX_FIGHTERS; i++)
    {
        SCPlayerData *pd = &gSCManagerTransferBattleState.players[i];
        SCVSIntroSlot *s;
        if (pd->pkind != nFTPlayerKindMan && pd->pkind != nFTPlayerKindCom)
        {
            continue;
        }
        s = &sSCVSIntroSlots[n];
        memset(s, 0, sizeof *s);
        s->player = i;
        s->fkind = pd->fkind;
        s->costume = pd->costume;
        s->shade = pd->shade;
        s->pkind = pd->pkind;
        n++;
    }
    sSCVSIntroSlotsNum = n;

    /* <P1> <VS> <P2> <P3> <P4>: one VS gap after the first fighter, every
     * fighter the same width (two players reproduce the 1P card's 135 px
     * decal position exactly) */
    w = (SCVSINTRO_VIEW_R - SCVSINTRO_VIEW_L - SCVSINTRO_VS_W) / (n > 0 ? n : 1);
    for (i = 0; i < n; i++)
    {
        s32 x0 = SCVSINTRO_VIEW_L + i * w + ((i > 0) ? SCVSINTRO_VS_W : 0);
        sSCVSIntroSlots[i].x0 = x0;
        sSCVSIntroSlots[i].x1 = (i == n - 1) ? SCVSINTRO_VIEW_R : x0 + w;
    }

    sSCVSIntroTotalTics = 0;
    sSCVSIntroAnnounceStep = 0;
    sSCVSIntroAnnounceNextTic = 2;
    sSCVSIntroFinishTic = -1;
}

/* ------------------------------------------------------------------ */
/*  Backdrop (1P intro assets)                                         */
/* ------------------------------------------------------------------ */

static void scVSIntroFuncLights(Gfx **dls)
{
    gSPDisplayList(dls[0]++, dSCVSIntroDisplayList);
}

static SObj *scVSIntroMakeSprite(GObj *gobj, void *file, intptr_t offset, f32 x, f32 y, sb32 transparent)
{
    SObj *sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, file, offset));
    if (transparent)
    {
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;
    }
    sobj->pos.x = x;
    sobj->pos.y = y;
    return sobj;
}

static void scVSIntroMakeBackdrop(void)
{
    GObj *gobj;

    /* sky (behind everything) */
    gobj = gcMakeGObjSPAfter(0, NULL, 17, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 26, GOBJ_PRIORITY_DEFAULT, ~0);
    scVSIntroMakeSprite(gobj, sSCVSIntroFiles[0], llSC1PIntroSkySprite, 10.0F, 59.0F, FALSE);

    /* top / bottom banners (over the fighters) */
    gobj = gcMakeGObjSPAfter(0, NULL, 18, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 28, GOBJ_PRIORITY_DEFAULT, ~0);
    scVSIntroMakeSprite(gobj, sSCVSIntroFiles[0], llSC1PIntroBannerTopSprite, 10.0F, 10.0F, FALSE);
    scVSIntroMakeSprite(gobj, sSCVSIntroFiles[0], llSC1PIntroBannerBottomSprite, 10.0F, 182.0F, FALSE);

    /* the VS decal in the gap after the first fighter */
    gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);
    {
        SObj *sobj = scVSIntroMakeSprite(gobj, sSCVSIntroFiles[0], llSC1PIntroVSDecalSprite, 135.0F, 104.0F, FALSE);
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT;
        if (sSCVSIntroSlotsNum > 0)
        {
            sobj->pos.x = (f32)sSCVSIntroSlots[0].x1 + ((f32)SCVSINTRO_VS_W - (f32)sobj->sprite.width) / 2.0F;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Names — full names in the results-screen announce letters          */
/* ------------------------------------------------------------------ */

static const char *dSCVSIntroVanillaNames[12] =
{
    "MARIO", "FOX", "DONKEY KONG", "SAMUS", "LUIGI", "LINK",
    "YOSHI", "CAPTAIN FALCON", "KIRBY", "PIKACHU", "JIGGLYPUFF", "NESS"
};

/* announce letter advance per glyph at scale 1 (mnvsresults.c) */
static const f32 dSCVSIntroLetterWidths[28] =
{
    35.0F, 24.0F, 24.0F, 28.0F, 22.0F, 20.0F, 31.0F, 27.0F, 9.0F, 20.0F, 27.0F, 20.0F, 37.0F, 29.0F,
    34.0F, 24.0F, 37.0F, 27.0F, 24.0F, 24.0F, 26.0F, 28.0F, 39.0F, 31.0F, 29.0F, 30.0F, 10.0F, 8.0F
};
#define SCVSINTRO_LETTER_H 26.0F
#define SCVSINTRO_SPACE_W 10.0F

static s32 scVSIntroGlyphID(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c == '!') return 26;
    if (c == '.') return 27;
    return -1;
}

static f32 scVSIntroTextWidth(const char *str)
{
    f32 w = 0.0F;
    for (; *str != '\0'; str++)
    {
        s32 g = scVSIntroGlyphID(*str);
        if (g >= 0) w += dSCVSIntroLetterWidths[g];
        else if (*str == ' ') w += SCVSINTRO_SPACE_W;
    }
    return w;
}

static void scVSIntroMakeText(GObj *gobj, const char *str, f32 x, f32 y, f32 scale)
{
    static const intptr_t offsets[28] =
    {
        llIFCommonAnnounceCommonLetterASprite, llIFCommonAnnounceCommonLetterBSprite,
        llIFCommonAnnounceCommonLetterCSprite, llIFCommonAnnounceCommonLetterDSprite,
        llIFCommonAnnounceCommonLetterESprite, llIFCommonAnnounceCommonLetterFSprite,
        llIFCommonAnnounceCommonLetterGSprite, llIFCommonAnnounceCommonLetterHSprite,
        llIFCommonAnnounceCommonLetterISprite, llIFCommonAnnounceCommonLetterJSprite,
        llIFCommonAnnounceCommonLetterKSprite, llIFCommonAnnounceCommonLetterLSprite,
        llIFCommonAnnounceCommonLetterMSprite, llIFCommonAnnounceCommonLetterNSprite,
        llIFCommonAnnounceCommonLetterOSprite, llIFCommonAnnounceCommonLetterPSprite,
        llIFCommonAnnounceCommonLetterQSprite, llIFCommonAnnounceCommonLetterRSprite,
        llIFCommonAnnounceCommonLetterSSprite, llIFCommonAnnounceCommonLetterTSprite,
        llIFCommonAnnounceCommonLetterUSprite, llIFCommonAnnounceCommonLetterVSprite,
        llIFCommonAnnounceCommonLetterWSprite, llIFCommonAnnounceCommonLetterXSprite,
        llIFCommonAnnounceCommonLetterYSprite, llIFCommonAnnounceCommonLetterZSprite,
        llIFCommonAnnounceCommonSymbolExclaimSprite,
        llIFCommonAnnounceCommonSymbolPeriodSprite
    };
    f32 cx = x;
    for (; *str != '\0'; str++)
    {
        s32 g = scVSIntroGlyphID(*str);
        SObj *sobj;
        if (g < 0)
        {
            if (*str == ' ') cx += SCVSINTRO_SPACE_W * scale;
            continue;
        }
        sobj = lbCommonMakeSObjForGObj(gobj, lbRelocGetFileData(Sprite*, sSCVSIntroFiles[1], offsets[g]));
        sobj->sprite.attr &= ~SP_FASTCOPY;
        sobj->sprite.attr |= SP_TRANSPARENT | SP_SCALE;
        sobj->sprite.scalex = scale;
        sobj->sprite.scaley = scale;
        sobj->pos.x = cx;
        sobj->pos.y = (g == 27) ? y + 26.0F * scale : y;
        /* white face, black outline (results-screen colour id 4) */
        sobj->envcolor.r = 0x00;
        sobj->envcolor.g = 0x00;
        sobj->envcolor.b = 0x00;
        sobj->sprite.red = 0xFF;
        sobj->sprite.green = 0xFF;
        sobj->sprite.blue = 0xFF;
        cx += dSCVSIntroLetterWidths[g] * scale;
    }
}

/* Up to two lines per fighter (split at the last space that balances the
 * halves), each scaled to fit its slice, centred on the bottom banner. */
static void scVSIntroMakeNames(void)
{
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    s32 i;

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < sSCVSIntroSlotsNum; i++)
    {
        SCVSIntroSlot *s = &sSCVSIntroSlots[i];
        const char *name = port_roster_player_display(s->player);
        char lines[2][48];
        s32 nlines = 1, k, n;
        f32 avail = (f32)(s->x1 - s->x0) - 8.0F;
        f32 scale = 1.0F, w0, w1, wmax, y;

        if (name[0] == '\0')
        {
            name = ((u32)s->fkind < 12) ? dSCVSIntroVanillaNames[s->fkind] : "";
        }
        for (n = 0; name[n] != '\0' && n < (s32)sizeof lines[0] - 1; n++) lines[0][n] = name[n];
        lines[0][n] = '\0';
        lines[1][0] = '\0';

        /* one line if it fits at a readable size, else split at a space */
        if (scVSIntroTextWidth(lines[0]) * 0.55F > avail)
        {
            s32 best = -1;
            f32 best_diff = 1e9F;
            for (k = 1; k < n - 1; k++)
            {
                if (lines[0][k] == ' ')
                {
                    f32 d;
                    lines[0][k] = '\0';
                    d = scVSIntroTextWidth(lines[0]) - scVSIntroTextWidth(&lines[0][k + 1]);
                    lines[0][k] = ' ';
                    if (d < 0.0F) d = -d;
                    if (d < best_diff) { best_diff = d; best = k; }
                }
            }
            if (best > 0)
            {
                for (k = 0; lines[0][best + 1 + k] != '\0'; k++) lines[1][k] = lines[0][best + 1 + k];
                lines[1][k] = '\0';
                lines[0][best] = '\0';
                nlines = 2;
            }
        }
        w0 = scVSIntroTextWidth(lines[0]);
        w1 = scVSIntroTextWidth(lines[1]);
        wmax = (w0 > w1) ? w0 : w1;
        if (wmax > 0.0F && wmax * scale > avail) scale = avail / wmax;
        if (nlines == 2 && scale > 0.7F) scale = 0.7F;
        if (nlines == 1 && scale > 0.9F) scale = 0.9F;

        y = (nlines == 1) ? 206.0F - SCVSINTRO_LETTER_H * scale / 2.0F
                          : 206.0F - SCVSINTRO_LETTER_H * scale - 2.0F;
        scVSIntroMakeText(gobj, lines[0], (f32)s->x0 + ((f32)(s->x1 - s->x0) - w0 * scale) / 2.0F, y, scale);
        if (nlines == 2)
        {
            scVSIntroMakeText(gobj, lines[1], (f32)s->x0 + ((f32)(s->x1 - s->x0) - w1 * scale) / 2.0F,
                              y + SCVSINTRO_LETTER_H * scale + 4.0F, scale);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Fighters + cameras                                                 */
/* ------------------------------------------------------------------ */

static void scVSIntroFighterProcUpdate(GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *dobj = DObjGetStruct(fighter_gobj);
    SCVSIntroSlot *s = ((u32)fp->card_anim_frame_id < (u32)sSCVSIntroSlotsNum)
                       ? &sSCVSIntroSlots[fp->card_anim_frame_id] : NULL;
    f32 step;

    if (s == NULL)
    {
        return;
    }
    /* slide in from the outside of the slice */
    step = s->half_w * 0.09F;
    if (dobj->translate.vec.f.x < 0.0F)
    {
        dobj->translate.vec.f.x += step;
        if (dobj->translate.vec.f.x > 0.0F) dobj->translate.vec.f.x = 0.0F;
    }
    else if (dobj->translate.vec.f.x > 0.0F)
    {
        dobj->translate.vec.f.x -= step;
        if (dobj->translate.vec.f.x < 0.0F) dobj->translate.vec.f.x = 0.0F;
    }
}

static void scVSIntroMakeFighterCamera(SCVSIntroSlot *s, s32 idx)
{
    static const s32 dl_links[SCVSINTRO_MAX_FIGHTERS] = { 29, 30, 31, 32 };
    GObj *gobj;
    CObj *cobj;
    CObjDesc desc;
    f32 aty, dist, aspect;

    gobj = gcMakeCameraGObj
    (
        nGCCommonKindSceneCamera,
        NULL,
        16,
        GOBJ_PRIORITY_DEFAULT,
        func_80017EC0,
        50 + idx,
        COBJ_MASK_DLLINK(dl_links[idx]),
        -1,
        FALSE,
        nGCProcessKindFunc,
        NULL,
        1,
        FALSE
    );
    gcAddXObjForCamera(CObjGetStruct(gobj), nGCMatrixKindPerspFastF, 0);
    gcAddXObjForCamera(CObjGetStruct(gobj), 7, 0);

    cobj = CObjGetStruct(gobj);
    syRdpSetViewport(&cobj->viewport, (f32)s->x0, (f32)SCVSINTRO_VIEW_T, (f32)s->x1, (f32)SCVSINTRO_VIEW_B);
    aspect = (f32)(s->x1 - s->x0) / (f32)(SCVSINTRO_VIEW_B - SCVSINTRO_VIEW_T);

    /* look-at height from the 1P card camera of this fighter kind */
    sc1PIntroGetFighterCObjDesc(&desc, ((u32)s->fkind < 12) ? s->fkind : 0, 0);
    aty = desc.at.y;
    if (aty < 120.0F) aty = 120.0F;
    dist = aty * SCVSINTRO_DIST_PER_ATY;

    /* results-screen convention: camera on +z, fighters face it */
    cobj->vec.at.x = 0.0F;
    cobj->vec.at.y = aty * 1.05F;
    cobj->vec.at.z = 0.0F;
    cobj->vec.eye.x = 0.0F;
    cobj->vec.eye.y = aty * 1.05F + dist * 0.05F;
    cobj->vec.eye.z = dist;

    cobj->projection.persp.fovy = 30.0F;
    cobj->projection.persp.aspect = aspect;
    cobj->projection.persp.near = 128.0F;
    cobj->projection.persp.far = 16384.0F;
    cobj->flags |= COBJ_FLAG_ZBUFFER;

    /* world half-width visible at the fighter's depth */
    s->half_w = dist * 0.2679F * aspect;   /* tan(15 deg) */
    s->camera_gobj = gobj;
}

static void scVSIntroMakeFighter(SCVSIntroSlot *s, s32 idx)
{
    static const s32 dl_links[SCVSINTRO_MAX_FIGHTERS] = { 29, 30, 31, 32 };
    FTDesc desc = dFTManagerDefaultFighterDesc;
    GObj *fighter_gobj;
    FTStruct *fp;
    DObj *dobj;

    desc.fkind = s->fkind;
    desc.costume = s->costume;
    desc.shade = s->shade;
    desc.figatree_heap = sSCVSIntroFigatreeHeaps[idx];
    /* the port keys mesh / UI / voice injection by player */
    desc.player = s->player;

    fighter_gobj = ftManagerMakeFighter(&desc);
    /* results-screen victory pose (Kirby only has two) */
    {
        static const s32 wins[3] = { nFTDemoStatusWin1, nFTDemoStatusWin2, nFTDemoStatusWin3 };
        scSubsysFighterSetStatus(fighter_gobj, wins[syUtilsRandIntRange((s->fkind == nFTKindKirby) ? 2 : 3)]);
    }

    fp = ftGetStruct(fighter_gobj);
    fp->card_anim_frame_id = idx;

    gcMoveGObjDL(fighter_gobj, dl_links[idx], GOBJ_PRIORITY_DEFAULT);
    gcAddGObjProcess(fighter_gobj, scVSIntroFighterProcUpdate, nGCProcessKindFunc, 1);

    dobj = DObjGetStruct(fighter_gobj);
    /* left half slides in from the left, right half from the right */
    dobj->translate.vec.f.x = (idx < (sSCVSIntroSlotsNum + 1) / 2) ? -s->half_w * 1.6F : s->half_w * 1.6F;
    dobj->translate.vec.f.y = 0.0F;
    dobj->translate.vec.f.z = 0.0F;
    dobj->scale.vec.f.x = 1.0F;
    dobj->scale.vec.f.y = 1.0F;
    dobj->scale.vec.f.z = 1.0F;

    s->fighter_gobj = fighter_gobj;
}

/* ------------------------------------------------------------------ */
/*  Announcer: A ... versus ... B, C, D                                 */
/* ------------------------------------------------------------------ */

static void scVSIntroUpdateAnnounce(void)
{
    /* steps: 0 = first name, 1 = "versus", 2.. = the other names, then done */
    s32 last = sSCVSIntroSlotsNum + 1;

    if (sSCVSIntroAnnounceStep > last || sSCVSIntroTotalTics < sSCVSIntroAnnounceNextTic)
    {
        return;
    }
    if (sSCVSIntroAnnounceStep == last)
    {
        /* sequence done: linger, then start the match */
        sSCVSIntroFinishTic = sSCVSIntroTotalTics + 70;
    }
    else if (sSCVSIntroAnnounceStep == 1)
    {
        gPortFighterVoiceMute = 0;
        func_800269C0_275C0(nSYAudioVoiceAnnounceVersus);
        gPortFighterVoiceMute = 1;
        sSCVSIntroAnnounceNextTic = sSCVSIntroTotalTics + 30;
    }
    else
    {
        s32 slot = (sSCVSIntroAnnounceStep == 0) ? 0 : sSCVSIntroAnnounceStep - 1;
        SCVSIntroSlot *s = &sSCVSIntroSlots[slot];
        sSCVSIntroAnnounceNextTic = sSCVSIntroTotalTics + port_voice_announce_player(s->player, s->fkind);
    }
    sSCVSIntroAnnounceStep++;
}

/* ------------------------------------------------------------------ */
/*  Scene loop                                                         */
/* ------------------------------------------------------------------ */

static void scVSIntroExit(void)
{
    gPortFighterVoiceMute = 0;
    portVoiceInjectStop();
    func_800266A0_272A0();
    /* scmanager continues straight into scVSBattleStartScene; keep the
     * scene id the battle code expects */
    gSCManagerSceneData.scene_curr = nSCKindVSBattle;
    syTaskmanSetLoadScene();
}

static void scVSIntroFuncRun(GObj *gobj)
{
    sSCVSIntroTotalTics++;
    scVSIntroUpdateAnnounce();

    if (sSCVSIntroTotalTics >= 45 &&
        scSubsysControllerGetPlayerTapButtons(A_BUTTON | B_BUTTON | START_BUTTON) != FALSE)
    {
        scVSIntroExit();
        return;
    }
    if (sSCVSIntroFinishTic >= 0 && sSCVSIntroTotalTics >= sSCVSIntroFinishTic)
    {
        scVSIntroExit();
        return;
    }
    if (sSCVSIntroTotalTics > 20 * 60)
    {
        scVSIntroExit();
    }
}

static void scVSIntroFuncStart(void)
{
    LBRelocSetup rl_setup;
    s32 i;

    rl_setup.table_addr = (uintptr_t)&lLBRelocTableAddr;
    rl_setup.table_files_num = (u32)llRelocFileCount;
    rl_setup.file_heap = NULL;
    rl_setup.file_heap_size = 0;
    rl_setup.status_buffer = sSCVSIntroStatusBuffer;
    rl_setup.status_buffer_size = ARRAY_COUNT(sSCVSIntroStatusBuffer);
    rl_setup.force_status_buffer = sSCVSIntroForceStatusBuffer;
    rl_setup.force_status_buffer_size = ARRAY_COUNT(sSCVSIntroForceStatusBuffer);

    lbRelocInitSetup(&rl_setup);
    lbRelocLoadFilesListed(dSCVSIntroFileIDs, sSCVSIntroFiles);

    gcMakeGObjSPAfter(0, scVSIntroFuncRun, 0, GOBJ_PRIORITY_DEFAULT);
    gcMakeDefaultCameraGObj(0, GOBJ_PRIORITY_DEFAULT, 100, COBJ_FLAG_FILLCOLOR | COBJ_FLAG_ZBUFFER, GPACK_RGBA8888(0x00, 0x00, 0x00, 0xFF));

    scVSIntroInitVars();
    gPortFighterVoiceMute = 1;
    efParticleInitAll();
    efManagerInitEffects();
    ftManagerAllocFighter(FTDATA_FLAG_SUBMOTION, sSCVSIntroSlotsNum);
    for (i = 0; i < sSCVSIntroSlotsNum; i++)
    {
        ftManagerSetupFilesAllKind(sSCVSIntroSlots[i].fkind);
        sSCVSIntroFigatreeHeaps[i] = syTaskmanMalloc(gFTManagerFigatreeHeapSize, 0x10);
    }

    /* sprite cameras: pictures (sky) / decals / banners, as the 1P intro */
    {
        static const struct { u32 prio; u32 link; } cams[] = { { 80, 26 }, { 20, 27 }, { 30, 28 } };
        s32 c;
        for (c = 0; c < 3; c++)
        {
            CObj *cobj = CObjGetStruct(gcMakeCameraGObj(nGCCommonKindSceneCamera, NULL, 16, GOBJ_PRIORITY_DEFAULT,
                                                        lbCommonDrawSprite, cams[c].prio, COBJ_MASK_DLLINK(cams[c].link),
                                                        ~0, FALSE, nGCProcessKindFunc, NULL, 1, FALSE));
            syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
        }
    }
    scVSIntroMakeBackdrop();
    scVSIntroMakeNames();
    for (i = 0; i < sSCVSIntroSlotsNum; i++)
    {
        scVSIntroMakeFighterCamera(&sSCVSIntroSlots[i], i);
        scVSIntroMakeFighter(&sSCVSIntroSlots[i], i);
    }
    /* Cameras draw highest priority first; the fighter slices (50+) leave
     * their scissor behind for the sprite cameras (30/20). An empty
     * full-frame 3D camera in between restores the full scissor. */
    {
        CObj *cobj = CObjGetStruct(gcMakeCameraGObj(nGCCommonKindSceneCamera, NULL, 16, GOBJ_PRIORITY_DEFAULT,
                                                    func_80017EC0, 40, COBJ_MASK_DLLINK(35),
                                                    -1, FALSE, nGCProcessKindFunc, NULL, 1, FALSE));
        gcAddXObjForCamera(cobj, nGCMatrixKindPerspFastF, 0);
        gcAddXObjForCamera(cobj, 7, 0);
        syRdpSetViewport(&cobj->viewport, 10.0F, 10.0F, 310.0F, 230.0F);
    }
    scSubsysFighterSetLightParams(-20.0F, 30.0F, 0xFF, 0xFF, 0xFF, 0xFF);

    syAudioPlayBGM(0, nSYAudioBGM1PIntro);
    sySchedulerSetTicCount(0);
}

static SYVideoSetup dSCVSIntroVideoSetup = SYVIDEO_SETUP_DEFAULT();

static SYTaskmanSetup dSCVSIntroTaskmanSetup =
{
    {
        0,
        gcRunAll,
        scManagerFuncDraw,
        &ovl24_BSS_END,
        0,
        1,
        2,
        sizeof(Gfx) * 6250,
        sizeof(Gfx) * 128,
        0,
        0,
        0x10000,
        2,
        0xC000,
        scVSIntroFuncLights,
        syControllerFuncRead,
    },
    0,
    sizeof(u64) * 192,
    0,
    0,
    0,
    0,
    sizeof(GObj),
    0,
    dLBCommonFuncMatrixList,
    NULL,
    0,
    0,
    0,
    sizeof(DObj),
    0,
    sizeof(SObj),
    0,
    sizeof(CObj),
    scVSIntroFuncStart
};

/* Runs the whole card (blocking, like every scene start function) and
 * returns when it is dismissed. Does nothing for a match with fewer than
 * two fighters. */
void scVSIntroStartScene(void)
{
    s32 i, n = 0;

    for (i = 0; i < GMCOMMON_PLAYERS_MAX; i++)
    {
        u8 pk = gSCManagerTransferBattleState.players[i].pkind;
        if (pk == nFTPlayerKindMan || pk == nFTPlayerKindCom) n++;
    }
    if (n < 2)
    {
        return;
    }
    /* Own scene id while the card runs: the port's mesh-slot bookkeeping
     * keys owner liveness on the scene id, and the card's fighters die
     * with this scene's arena. scVSIntroExit restores nSCKindVSBattle. */
    gSCManagerSceneData.scene_curr = nSCKindVSIntro;

    dSCVSIntroVideoSetup.zbuffer = SYVIDEO_ZBUFFER_START(320, 240, 0, 10, u16);
    syVideoInit(&dSCVSIntroVideoSetup);

    dSCVSIntroTaskmanSetup.scene_setup.arena_size = (size_t)((uintptr_t)&ovl1_VRAM - (uintptr_t)&ovl24_BSS_END);
    scManagerFuncUpdate(&dSCVSIntroTaskmanSetup);
}

#endif /* PORT */
