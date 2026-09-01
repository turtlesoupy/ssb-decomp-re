#ifdef PORT

#include <it/item.h>
#include <ft/fighter.h>
#include <PR/gbi.h>
#include <stddef.h>
#include <stdio.h>

extern void *malloc(size_t size);
extern void free(void *ptr);
extern char *getenv(const char *name);
extern void port_log(const char *fmt, ...);

#define ITPORT_OSB2_BATCH_MAX 4096
#define ITPORT_OSB2_VERTS_BATCH_MAX 30
#define ITPORT_OSB2_TRIS_BATCH_MAX 512
#define ITPORT_OSB2_TOTAL_MAX (1024 * 1024)
/* The stock drawable transform lands its model origin on the lower edge of
 * Mario's glove. Generated assets define their semantic grip at local zero,
 * so lift that canonical point to the visual center of the hand. */
#define ITPORT_BAT_HAND_ANCHOR_Y 30.0F

typedef struct ITPortOSB2Vert
{
    s16 x;
    s16 y;
    s16 z;
    u8 r;
    u8 g;
    u8 b;
    u8 pad;
} ITPortOSB2Vert;

typedef struct ITPortOSB4Vert
{
    s16 x;
    s16 y;
    s16 z;
    s16 s;
    s16 t;
    s8 nx;
    s8 ny;
    s8 nz;
    u8 pad;
} ITPortOSB4Vert;

static Gfx *sITPortBatDisplayList;
static sb32 sITPortBatLoadAttempted;
static sb32 sITPortBatBaseTranslateYSet;
static f32 sITPortBatBaseTranslateY;

static sb32 itPortReadU32Pair(FILE *file, u32 values[2])
{
    return (fread(values, sizeof(u32), 2, file) == 2) ? TRUE : FALSE;
}

/* OSB2 is deliberately the MVP format: opaque, vertex-colored geometry with
 * triangles pre-batched for the RSP's 30-vertex window. The generated Gfx and
 * Vtx allocations are process-lifetime data, matching the existing fighter
 * injection cache. */
static Gfx *itPortBuildOSB2DisplayList(FILE *file, u32 batches_num)
{
    Gfx *display_list;
    Gfx *gfx;
    Vtx *vertices;
    u32 total_vertices = 0;
    u32 total_triangles = 0;
    u32 batch;
    u32 vertex_offset;
    long batches_offset;

    if ((batches_num == 0) || (batches_num > ITPORT_OSB2_BATCH_MAX))
    {
        return NULL;
    }
    batches_offset = ftell(file);
    if (batches_offset < 0)
    {
        return NULL;
    }

    for (batch = 0; batch < batches_num; batch++)
    {
        u32 header[2];
        long payload_size;

        if (!itPortReadU32Pair(file, header) ||
            (header[0] == 0) || (header[0] > ITPORT_OSB2_VERTS_BATCH_MAX) ||
            (header[1] == 0) || (header[1] > ITPORT_OSB2_TRIS_BATCH_MAX) ||
            (total_vertices > ITPORT_OSB2_TOTAL_MAX - header[0]) ||
            (total_triangles > ITPORT_OSB2_TOTAL_MAX - header[1]))
        {
            return NULL;
        }
        total_vertices += header[0];
        total_triangles += header[1];
        payload_size = (long)(header[0] * sizeof(ITPortOSB2Vert) + header[1] * 4);

        if (fseek(file, payload_size, SEEK_CUR) != 0)
        {
            return NULL;
        }
    }
    if (fseek(file, batches_offset, SEEK_SET) != 0)
    {
        return NULL;
    }

    vertices = malloc(sizeof(Vtx) * total_vertices);
    display_list = malloc(sizeof(Gfx) * (16 + batches_num + total_triangles));
    if ((vertices == NULL) || (display_list == NULL))
    {
        free(vertices);
        free(display_list);
        return NULL;
    }

    gfx = display_list;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPClearGeometryMode(gfx++, G_LIGHTING | G_TEXTURE_GEN | G_CULL_BOTH);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);

    vertex_offset = 0;
    for (batch = 0; batch < batches_num; batch++)
    {
        ITPortOSB2Vert source_vertices[ITPORT_OSB2_VERTS_BATCH_MAX];
        u8 source_triangles[ITPORT_OSB2_TRIS_BATCH_MAX * 4];
        u32 header[2];
        u32 i;

        if (!itPortReadU32Pair(file, header) ||
            (fread(source_vertices, sizeof(ITPortOSB2Vert), header[0], file) != header[0]) ||
            (fread(source_triangles, 4, header[1], file) != header[1]))
        {
            free(vertices);
            free(display_list);
            return NULL;
        }
        for (i = 0; i < header[0]; i++)
        {
            Vtx *vertex = &vertices[vertex_offset + i];

            vertex->v.ob[0] = source_vertices[i].x;
            vertex->v.ob[1] = source_vertices[i].y;
            vertex->v.ob[2] = source_vertices[i].z;
            vertex->v.flag = 0;
            vertex->v.tc[0] = 0;
            vertex->v.tc[1] = 0;
            vertex->v.cn[0] = source_vertices[i].r;
            vertex->v.cn[1] = source_vertices[i].g;
            vertex->v.cn[2] = source_vertices[i].b;
            vertex->v.cn[3] = 0xFF;
        }
        gSPVertex(gfx++, &vertices[vertex_offset], header[0], 0);

        for (i = 0; i < header[1]; i++)
        {
            u8 *triangle = &source_triangles[i * 4];

            if ((triangle[0] >= header[0]) || (triangle[1] >= header[0]) || (triangle[2] >= header[0]))
            {
                free(vertices);
                free(display_list);
                return NULL;
            }
            gSP1Triangle(gfx++, triangle[0], triangle[1], triangle[2], 0);
        }
        vertex_offset += header[0];
    }

    gDPPipeSync(gfx++);
    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPEndDisplayList(gfx++);

    return display_list;
}

/* Textured-lit items use the same compact vertex treatment proven by the
 * generated-fighter OSB4 path: a small RGBA16 atlas, welded smooth normals,
 * and neutral ambient/key lights. This avoids turning provider UV seams and
 * baked AO into interpolated vertex-color streaks. */
static Gfx *itPortBuildOSB4DisplayList(FILE *file, u32 batches_num,
                                      u8 *texture, u32 texture_width,
                                      u32 texture_height)
{
    Gfx *display_list;
    Gfx *gfx;
    Vtx *vertices;
    u32 total_vertices = 0;
    u32 total_triangles = 0;
    u32 batch;
    u32 vertex_offset;
    long batches_offset;
    static Lights1 sITPortItemLights = gdSPDefLights1(
        145, 145, 145,
        255, 255, 255,
        45, 95, 70);

    if ((batches_num == 0) || (batches_num > ITPORT_OSB2_BATCH_MAX) ||
        (texture == NULL) || (texture_width < 4) || (texture_width > 1024) ||
        (texture_height < 4) || (texture_height > 1024))
    {
        return NULL;
    }
    batches_offset = ftell(file);
    if (batches_offset < 0)
    {
        return NULL;
    }

    for (batch = 0; batch < batches_num; batch++)
    {
        u32 header[2];
        long payload_size;

        if (!itPortReadU32Pair(file, header) ||
            (header[0] == 0) || (header[0] > ITPORT_OSB2_VERTS_BATCH_MAX) ||
            (header[1] == 0) || (header[1] > ITPORT_OSB2_TRIS_BATCH_MAX) ||
            (total_vertices > ITPORT_OSB2_TOTAL_MAX - header[0]) ||
            (total_triangles > ITPORT_OSB2_TOTAL_MAX - header[1]))
        {
            return NULL;
        }
        total_vertices += header[0];
        total_triangles += header[1];
        payload_size = (long)(header[0] * sizeof(ITPortOSB4Vert) + header[1] * 4);
        if (fseek(file, payload_size, SEEK_CUR) != 0)
        {
            return NULL;
        }
    }
    if (fseek(file, batches_offset, SEEK_SET) != 0)
    {
        return NULL;
    }

    vertices = malloc(sizeof(Vtx) * total_vertices);
    display_list = malloc(sizeof(Gfx) * (32 + batches_num + total_triangles));
    if ((vertices == NULL) || (display_list == NULL))
    {
        free(vertices);
        free(display_list);
        return NULL;
    }

    gfx = display_list;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPClearGeometryMode(gfx++, G_TEXTURE_GEN | G_CULL_BOTH);
    gSPSetGeometryMode(gfx++, G_LIGHTING | G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
    gSPSetLights1(gfx++, sITPortItemLights);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, texture_width, texture);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (texture_width * 2) / 8, 0,
               G_TX_LOADTILE, 0,
               G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
    gDPLoadSync(gfx++);
    gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0,
                (texture_width - 1) << 2, (texture_height - 1) << 2);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, (texture_width * 2) / 8, 0,
               G_TX_RENDERTILE, 0,
               G_TX_CLAMP, 0, G_TX_NOLOD, G_TX_CLAMP, 0, G_TX_NOLOD);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0,
                   (texture_width - 1) << 2, (texture_height - 1) << 2);

    vertex_offset = 0;
    for (batch = 0; batch < batches_num; batch++)
    {
        ITPortOSB4Vert source_vertices[ITPORT_OSB2_VERTS_BATCH_MAX];
        u8 source_triangles[ITPORT_OSB2_TRIS_BATCH_MAX * 4];
        u32 header[2];
        u32 i;

        if (!itPortReadU32Pair(file, header) ||
            (fread(source_vertices, sizeof(ITPortOSB4Vert), header[0], file) != header[0]) ||
            (fread(source_triangles, 4, header[1], file) != header[1]))
        {
            free(vertices);
            free(display_list);
            return NULL;
        }
        for (i = 0; i < header[0]; i++)
        {
            Vtx *vertex = &vertices[vertex_offset + i];

            vertex->n.ob[0] = source_vertices[i].x;
            vertex->n.ob[1] = source_vertices[i].y;
            vertex->n.ob[2] = source_vertices[i].z;
            vertex->n.flag = 0;
            vertex->n.tc[0] = source_vertices[i].s;
            vertex->n.tc[1] = source_vertices[i].t;
            vertex->n.n[0] = source_vertices[i].nx;
            vertex->n.n[1] = source_vertices[i].ny;
            vertex->n.n[2] = source_vertices[i].nz;
            vertex->n.a = 0xFF;
        }
        gSPVertex(gfx++, &vertices[vertex_offset], header[0], 0);

        for (i = 0; i < header[1]; i++)
        {
            u8 *triangle = &source_triangles[i * 4];

            if ((triangle[0] >= header[0]) || (triangle[1] >= header[0]) ||
                (triangle[2] >= header[0]))
            {
                free(vertices);
                free(display_list);
                return NULL;
            }
            gSP1Triangle(gfx++, triangle[0], triangle[1], triangle[2], 0);
        }
        vertex_offset += header[0];
    }

    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gSPSetGeometryMode(gfx++, G_LIGHTING | G_CULL_BACK);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    gSPEndDisplayList(gfx++);
    return display_list;
}

static Gfx *itPortLoadBatDisplayList(void)
{
    const char *path = getenv("SSB64_INJECT_ITEM_BAT");
    FILE *file;
    Gfx *display_list = NULL;
    u8 magic[4];
    u32 parts_num;
    u32 part_header[2];
    sb32 is_textured_lit;

    if ((path == NULL) || (path[0] == '\0'))
    {
        return NULL;
    }
    file = fopen(path, "rb");
    if (file == NULL)
    {
        port_log("ITEMOSB: cannot open bat override %s\n", path);
        return NULL;
    }
    if ((fread(magic, 1, sizeof(magic), file) != sizeof(magic)) ||
        (magic[0] != 'O') || (magic[1] != 'S') || (magic[2] != 'B') ||
        ((magic[3] != '2') && (magic[3] != '4')) ||
        (fread(&parts_num, sizeof(parts_num), 1, file) != 1) || (parts_num != 1))
    {
        port_log("ITEMOSB: %s must be a one-part OSB2 or OSB4 bundle\n", path);
        fclose(file);
        return NULL;
    }
    is_textured_lit = (magic[3] == '4') ? TRUE : FALSE;
    if (is_textured_lit)
    {
        u32 texture_size[2];
        size_t texture_bytes;
        u8 *texture;

        if (!itPortReadU32Pair(file, texture_size) ||
            (texture_size[0] < 4) || (texture_size[0] > 1024) ||
            (texture_size[1] < 4) || (texture_size[1] > 1024))
        {
            port_log("ITEMOSB: invalid OSB4 texture dimensions in %s\n", path);
            fclose(file);
            return NULL;
        }
        texture_bytes = (size_t)texture_size[0] * texture_size[1] * 2;
        texture = malloc(texture_bytes);
        if ((texture == NULL) || (fread(texture, 1, texture_bytes, file) != texture_bytes) ||
            !itPortReadU32Pair(file, part_header) || (part_header[0] != 0))
        {
            free(texture);
            port_log("ITEMOSB: invalid OSB4 payload in %s\n", path);
            fclose(file);
            return NULL;
        }
        display_list = itPortBuildOSB4DisplayList(
            file, part_header[1], texture, texture_size[0], texture_size[1]);
        if (display_list == NULL)
        {
            free(texture);
        }
    }
    else
    {
        if (!itPortReadU32Pair(file, part_header) || (part_header[0] != 0))
        {
            port_log("ITEMOSB: invalid OSB2 payload in %s\n", path);
            fclose(file);
            return NULL;
        }
        display_list = itPortBuildOSB2DisplayList(file, part_header[1]);
    }
    fclose(file);

    if (display_list == NULL)
    {
        port_log("ITEMOSB: invalid or unsupported bat override %s\n", path);
        return NULL;
    }
    port_log("ITEMOSB: loaded bat override %s\n", path);
    return display_list;
}

static DObj *itPortFindFirstDrawableDObj(DObj *dobj)
{
    while (dobj != NULL)
    {
        DObj *found;

        if (dobj->dv != NULL)
        {
            return dobj;
        }
        found = itPortFindFirstDrawableDObj(dobj->child);
        if (found != NULL)
        {
            return found;
        }
        dobj = dobj->sib_next;
    }
    return NULL;
}

static void itPortClearOtherDObjGeometry(DObj *dobj, DObj *keep)
{
    while (dobj != NULL)
    {
        if (dobj != keep)
        {
            dobj->dv = NULL;
        }
        itPortClearOtherDObjGeometry(dobj->child, keep);
        dobj = dobj->sib_next;
    }
}

void itPortApplyBatModelOverride(GObj *item_gobj)
{
    DObj *root;
    DObj *drawable;

    if (!sITPortBatLoadAttempted)
    {
        sITPortBatLoadAttempted = TRUE;
        sITPortBatDisplayList = itPortLoadBatDisplayList();
    }
    if (sITPortBatDisplayList == NULL)
    {
        return;
    }

    root = DObjGetStruct(item_gobj);
    /* Keep the stock bat's drawable child because it has the correct held-item
     * transform. The root can also contain stock geometry, though, so clear
     * the complete tree below rather than only the descendants. */
    drawable = itPortFindFirstDrawableDObj(root->child);
    if (drawable == NULL)
    {
        drawable = itPortFindFirstDrawableDObj(root);
    }
    if (drawable == NULL)
    {
        port_log("ITEMOSB: bat has no drawable DObj to replace\n");
        return;
    }

    if (!sITPortBatBaseTranslateYSet)
    {
        sITPortBatBaseTranslateYSet = TRUE;
        sITPortBatBaseTranslateY = drawable->translate.vec.f.y;
    }
    itPortClearOtherDObjGeometry(root, drawable);
    drawable->dl = sITPortBatDisplayList;
    drawable->flags &= ~(DOBJ_FLAG_NOTEXTURE | DOBJ_FLAG_HIDDEN);
    /* Preserve the stock drawable transform. In particular, the bat model's
     * Y translation is the engine-side grip calibration relative to the
     * fighter's item-light joint; generated geometry is authored around the
     * drawable's local origin and must inherit that same hand anchor. */
    /* itMainSetFighterHold restores the stock DObj descriptor, and swing setup
     * may do so again. Reassert from the cached pristine value instead of
     * accumulating a delta on repeated attacks. */
    drawable->translate.vec.f.y =
        sITPortBatBaseTranslateY + ITPORT_BAT_HAND_ANCHOR_Y;
}

/* The stock bat is nearly symmetric, so its drawable never exposes that the
 * held-item joint only mirrors the attachment position, not child geometry.
 * Generated asymmetric meshes do expose it: without this correction Mario
 * grips the handle while facing left and the opposite end while facing right.
 * Mirror around the item's attachment origin every display so turns made while
 * holding the item are handled as well. */
void itPortUpdateBatModelFacing(GObj *item_gobj)
{
    ITStruct *ip = itGetStruct(item_gobj);
    DObj *root;
    DObj *drawable;

    if ((sITPortBatDisplayList == NULL) || (ip->kind != nITKindBat))
    {
        return;
    }

    root = DObjGetStruct(item_gobj);
    drawable = itPortFindFirstDrawableDObj(root->child);
    if (drawable == NULL)
    {
        drawable = itPortFindFirstDrawableDObj(root);
    }
    if (drawable == NULL)
    {
        return;
    }

    if (ip->is_hold && (ip->owner_gobj != NULL))
    {
        FTStruct *fp = ftGetStruct(ip->owner_gobj);

        drawable->scale.vec.f.x = (fp->lr == +1) ? -1.0F : 1.0F;
    }
    else
    {
        drawable->scale.vec.f.x = 1.0F;
    }
}

#endif
