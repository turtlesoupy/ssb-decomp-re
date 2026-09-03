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
extern int atoi(const char *);
#include <string.h>
#include <math.h>

extern void func_800266A0_272A0();
extern void* func_800269C0_275C0(u16);
extern u32 sySchedulerGetTicCount();

/* ftport.c */
extern s32 port_voice_announce_player(s32 player, s32 fkind);
extern const char *port_roster_player_display(s32 player);
extern s32 port_osb5_mesh_bounds_local(void *fighter_gobj, f32 mn[3], f32 mx[3]);
/* port/audio/voice_inject */
extern void portVoiceInjectStop(void);
/* n_env.c: drop fighter voice lines (victory poses) while the card runs */
extern s32 gPortFighterVoiceMute;
/* sc1pintro.c — per-fighter look-at heights of the 1P card cameras */
extern CObjDesc* sc1PIntroGetFighterCObjDesc(CObjDesc *cobj_desc, s32 fkind, s32 cobj_id);
/* gmcollision.c: DObj chain -> world position */
extern void gmCollisionGetFighterPartsWorldPosition(DObj *dobj, Vec3f *pos);

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
#define SCVSINTRO_TAN_HALF_FOVY 0.2679F /* tan(15 deg) */
#define SCVSINTRO_LINGER_TICS 24        /* after the announcer chain ends */

typedef struct SCVSIntroSlot
{
    s32 player;
    s32 fkind;
    s32 costume;
    s32 shade;
    s32 pkind;
    s32 x0, x1;             /* viewport slice */
    f32 half_w;             /* world half-width visible at the fighter */
    f32 aspect;
    /* this frame's bounds of the posed fighter, and the largest size seen
     * (the dolly never pushes back in: stable framing through the pose) */
    sb32 bounds_have;
    f32 bx0, bx1, by0, by1;
    f32 wmax, hmax;
    /* smoothed camera */
    f32 cam_atx, cam_aty, cam_dist;
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

/* Greedy word-wrap of a name into at most SCVSINTRO_NAME_LINES lines at a
 * given scale. Returns FALSE when a line (or a single word) will not fit. */
#define SCVSINTRO_NAME_LINES 3
static s32 scVSIntroStrCopy(char *dst, const char *src, s32 cap)
{
    s32 n = 0;
    while (src[n] != '\0' && n < cap - 1) { dst[n] = src[n]; n++; }
    dst[n] = '\0';
    return n;
}
static sb32 scVSIntroWrapName(const char *name, f32 avail, f32 scale, char lines[SCVSINTRO_NAME_LINES][48], s32 *nlines)
{
    s32 n = 0, len = 0;
    const char *p = name;

    lines[0][0] = '\0';
    while (*p == ' ') p++;
    while (*p != '\0')
    {
        char word[48];
        s32 wl = 0;
        char probe[48];
        while (*p != '\0' && *p != ' ' && wl < 47) word[wl++] = *p++;
        word[wl] = '\0';
        while (*p == ' ') p++;
        if (n == 0 && len == 0) n = 1;
        if (len == 0)
        {
            scVSIntroStrCopy(probe, word, 48);
        }
        else if (len + 1 + wl < 48)
        {
            s32 m = scVSIntroStrCopy(probe, lines[n - 1], 48);
            probe[m++] = ' ';
            scVSIntroStrCopy(&probe[m], word, 48 - m);
        }
        else
        {
            return FALSE;
        }
        if (scVSIntroTextWidth(probe) * scale <= avail)
        {
            len = scVSIntroStrCopy(lines[n - 1], probe, 48);
        }
        else if (len == 0 || n >= SCVSINTRO_NAME_LINES)
        {
            return FALSE;
        }
        else
        {
            scVSIntroStrCopy(lines[n], word, 48);
            len = wl;
            n++;
        }
    }
    *nlines = (n > 0) ? n : 1;
    return TRUE;
}

/* Every name shares one scale (the largest at which all of them fit their
 * slice in at most three lines), centred on the bottom banner. */
static void scVSIntroMakeNames(void)
{
    static const f32 scales[] = { 0.9F, 0.8F, 0.7F, 0.6F, 0.55F, 0.5F, 0.45F, 0.4F, 0.35F, 0.3F, 0.25F };
    GObj *gobj = gcMakeGObjSPAfter(0, NULL, 19, GOBJ_PRIORITY_DEFAULT);
    const char *names[SCVSINTRO_MAX_FIGHTERS];
    char lines[SCVSINTRO_MAX_FIGHTERS][SCVSINTRO_NAME_LINES][48];
    s32 nlines[SCVSINTRO_MAX_FIGHTERS];
    f32 scale = scales[ARRAY_COUNT(scales) - 1];
    s32 i, si;

    gcAddGObjDisplay(gobj, lbCommonDrawSObjAttr, 27, GOBJ_PRIORITY_DEFAULT, ~0);

    for (i = 0; i < sSCVSIntroSlotsNum; i++)
    {
        SCVSIntroSlot *s = &sSCVSIntroSlots[i];
        names[i] = port_roster_player_display(s->player);
        if (names[i][0] == '\0')
        {
            names[i] = ((u32)s->fkind < 12) ? dSCVSIntroVanillaNames[s->fkind] : "";
        }
    }
    for (si = 0; si < (s32)ARRAY_COUNT(scales); si++)
    {
        sb32 ok = TRUE;
        for (i = 0; i < sSCVSIntroSlotsNum && ok; i++)
        {
            SCVSIntroSlot *s = &sSCVSIntroSlots[i];
            f32 avail = (f32)(s->x1 - s->x0) - 6.0F;
            ok = scVSIntroWrapName(names[i], avail, scales[si], lines[i], &nlines[i]);
        }
        if (ok)
        {
            scale = scales[si];
            break;
        }
    }
    if (si == (s32)ARRAY_COUNT(scales))
    {
        /* nothing fits even at the smallest scale: draw what wraps anyway */
        for (i = 0; i < sSCVSIntroSlotsNum; i++)
        {
            SCVSIntroSlot *s = &sSCVSIntroSlots[i];
            if (!scVSIntroWrapName(names[i], (f32)(s->x1 - s->x0) - 6.0F, scale, lines[i], &nlines[i]))
            {
                scVSIntroStrCopy(lines[i][0], names[i], 48);
                nlines[i] = 1;
            }
        }
    }
    for (i = 0; i < sSCVSIntroSlotsNum; i++)
    {
        SCVSIntroSlot *s = &sSCVSIntroSlots[i];
        f32 lh = SCVSINTRO_LETTER_H * scale;
        f32 gap = 4.0F * scale;
        f32 total = (f32)nlines[i] * lh + (f32)(nlines[i] - 1) * gap;
        f32 y = 206.0F - total / 2.0F;
        s32 k;
        for (k = 0; k < nlines[i]; k++)
        {
            f32 w = scVSIntroTextWidth(lines[i][k]);
            scVSIntroMakeText(gobj, lines[i][k], (f32)s->x0 + ((f32)(s->x1 - s->x0) - w * scale) / 2.0F, y, scale);
            y += lh + gap;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Fighters + cameras                                                 */
/* ------------------------------------------------------------------ */

/* Bounds of the posed fighter in its own frame (the slide-in offset
 * removed): every joint, plus the corners of the injected mesh's box when
 * one is skinned (those meshes reach well past the vanilla joints). */
static void scVSIntroMeasure(SCVSIntroSlot *s, GObj *fighter_gobj)
{
    FTStruct *fp = ftGetStruct(fighter_gobj);
    DObj *root = DObjGetStruct(fighter_gobj);
    f32 x0 = 1e9F, x1 = -1e9F, y0 = 1e9F, y1 = -1e9F;
    f32 mn[3], mx[3];
    s32 k, n = 0;

    for (k = 0; k < FTPARTS_JOINT_NUM_MAX; k++)
    {
        Vec3f pos;
        DObj *j = fp->joints[k];
        if (j == NULL) continue;
        pos.x = pos.y = pos.z = 0.0F;
        gmCollisionGetFighterPartsWorldPosition(j, &pos);
        pos.x -= root->translate.vec.f.x;
        if (!(pos.x > -1e6F && pos.x < 1e6F && pos.y > -1e6F && pos.y < 1e6F)) continue;
        if (pos.x < x0) x0 = pos.x;
        if (pos.x > x1) x1 = pos.x;
        if (pos.y < y0) y0 = pos.y;
        if (pos.y > y1) y1 = pos.y;
        n++;
    }
    if (fp->joints[0] != NULL && port_osb5_mesh_bounds_local(fighter_gobj, mn, mx))
    {
        for (k = 0; k < 8; k++)
        {
            Vec3f pos;
            pos.x = (k & 1) ? mx[0] : mn[0];
            pos.y = (k & 2) ? mx[1] : mn[1];
            pos.z = (k & 4) ? mx[2] : mn[2];
            gmCollisionGetFighterPartsWorldPosition(fp->joints[0], &pos);
            pos.x -= root->translate.vec.f.x;
            if (!(pos.x > -1e6F && pos.x < 1e6F && pos.y > -1e6F && pos.y < 1e6F)) continue;
            if (pos.x < x0) x0 = pos.x;
            if (pos.x > x1) x1 = pos.x;
            if (pos.y < y0) y0 = pos.y;
            if (pos.y > y1) y1 = pos.y;
            n++;
        }
    }
    else if (fp->attr != NULL)
    {
        /* vanilla mesh: the joints sit inside the body (Jigglypuff's
         * balloon, Kirby); grow the box by the collision diamond */
        f32 half_w = fp->attr->map_coll.width / 2.0F;
        f32 body_h = fp->attr->map_coll.top - fp->attr->map_coll.bottom;
        f32 sc = root->scale.vec.f.y;
        if (sc <= 0.0F) sc = 1.0F;
        x0 -= half_w * sc;
        x1 += half_w * sc;
        if (n > 0 && y1 - y0 < body_h * sc)
        {
            f32 grow = (body_h * sc - (y1 - y0)) / 2.0F;
            y0 -= grow;
            y1 += grow;
        }
    }
    if (n < 2 || y1 - y0 < 1.0F) return;
    s->bx0 = x0; s->bx1 = x1; s->by0 = y0; s->by1 = y1;
    if (!s->bounds_have)
    {
        s->wmax = x1 - x0;
        s->hmax = y1 - y0;
        s->bounds_have = TRUE;
    }
    else
    {
        if (x1 - x0 > s->wmax) s->wmax = x1 - x0;
        if (y1 - y0 > s->hmax) s->hmax = y1 - y0;
    }
}

/* Dolly/pan the slot camera so the running bounds (plus a margin for the
 * mesh volume around the joints) fill the slice, whatever the pose does. */
static void scVSIntroFitCamera(SCVSIntroSlot *s)
{
    CObj *cobj;
    f32 h, w, pad, dist_v, dist_h, dist, atx, aty, k;

    if (!s->bounds_have || s->camera_gobj == NULL) return;
    cobj = CObjGetStruct(s->camera_gobj);
    h = s->hmax;
    w = s->wmax;
    pad = h * 0.12F + 20.0F;
    dist_v = (h / 2.0F + pad) / SCVSINTRO_TAN_HALF_FOVY;
    dist_h = (w / 2.0F + pad) / (SCVSINTRO_TAN_HALF_FOVY * s->aspect);
    dist = (dist_v > dist_h) ? dist_v : dist_h;
    if (dist < 400.0F) dist = 400.0F;
    atx = (s->bx0 + s->bx1) / 2.0F;
    aty = (s->by0 + s->by1) / 2.0F;

    if (s->cam_dist <= 0.0F)
    {
        s->cam_atx = atx; s->cam_aty = aty; s->cam_dist = dist;
    }
    /* pan follows the pose gently; the dolly only ever pulls back */
    k = 0.15F;
    s->cam_atx += (atx - s->cam_atx) * k;
    s->cam_aty += (aty - s->cam_aty) * k;
    if (dist > s->cam_dist) s->cam_dist += (dist - s->cam_dist) * 0.3F;

    cobj->vec.at.x = s->cam_atx;
    cobj->vec.at.y = s->cam_aty;
    cobj->vec.at.z = 0.0F;
    cobj->vec.eye.x = s->cam_atx;
    cobj->vec.eye.y = s->cam_aty + s->cam_dist * 0.05F;
    cobj->vec.eye.z = s->cam_dist;
    s->half_w = s->cam_dist * SCVSINTRO_TAN_HALF_FOVY * s->aspect;
}

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
    scVSIntroMeasure(s, fighter_gobj);
    scVSIntroFitCamera(s);
    /* SSB64_VSINTRO_FREEZE_TIC=N: stop the pose animation N card tics in
     * (og_sprite.py: global frame numbers drift with load time, so the
     * shot frame is taken well after the freeze instead). */
    {
        static s32 sFreezeTic = -2;
        if (sFreezeTic == -2)
        {
            const char *e = getenv("SSB64_VSINTRO_FREEZE_TIC");
            sFreezeTic = (e != NULL) ? atoi(e) : -1;
        }
        if (sFreezeTic >= 0 && sSCVSIntroTotalTics >= sFreezeTic)
        {
            gcSetAnimSpeed(fighter_gobj, 0.0F);
        }
    }
    {
        /* SSB64_DUMP_FRAMES joint-frame dump (same hook battle uses) */
        extern void port_dump_frame(GObj *fighter_gobj);
        extern void ftParamsUpdateFighterPartsTransformAll(DObj *root_dobj);
        if (fp->joints[0] != NULL) ftParamsUpdateFighterPartsTransformAll(fp->joints[0]);
        port_dump_frame(fighter_gobj);
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

    /* world half-width visible at the fighter's depth (initial guess; the
     * fighter proc refits the camera to the posed joints every frame) */
    s->aspect = aspect;
    s->half_w = dist * SCVSINTRO_TAN_HALF_FOVY * aspect;
    s->bounds_have = FALSE;
    s->cam_dist = 0.0F;
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
    /* The per-kind "selected" victory pose the character-select card
     * uses (Samus/Fox Win4, Mario/Kirby Win3, ...). These are authored to
     * HOLD; the generic Win1..3 results poses loop a 16-tic tail with a
     * discontinuous restart (Samus's cannon snapped ~80 units every 16
     * frames on the card — "arm glitching up and down", 2026-09-03). */
    {
        extern s32 mnPlayersVSGetStatusSelected(s32 fkind);
        s32 status = mnPlayersVSGetStatusSelected(((u32)s->fkind < 12) ? s->fkind : 0);
        /* SSB64_VSINTRO_WIN=1..4: force a results pose (og_sprite.py: the
         * chibi Kirby/Purin "selected" poses bow into the camera and read as
         * a squashed head on a social card). */
        {
            const char *e = getenv("SSB64_VSINTRO_WIN");
            if (e != NULL && e[0] >= '1' && e[0] <= '4' && e[1] == '\0')
            {
                status = nFTDemoStatusWin1 + (e[0] - '1');
            }
            else if (e != NULL && e[0] == '0' && e[1] == '\0')
            {
                status = nFTCommonStatusWait; /* plain idle stance */
            }
        }
        scSubsysFighterSetStatus(fighter_gobj, status);
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
        sSCVSIntroFinishTic = sSCVSIntroTotalTics + SCVSINTRO_LINGER_TICS;
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
    /* SSB64_VSINTRO_HOLD=1: never auto-dismiss (og_sprite.py shoots late
     * frames of a results pose; SSB64_MAX_FRAMES ends the process). */
    if (getenv("SSB64_VSINTRO_HOLD") != NULL)
    {
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
