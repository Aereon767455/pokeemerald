#include "global.h"
#include "ui_outfits.h"
#include "strings.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "item.h"
#include "item_menu.h"
#include "item_menu_icons.h"
#include "list_menu.h"
#include "item_icon.h"
#include "item_use.h"
#include "international_string_util.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "party_menu.h"
#include "scanline_effect.h"
#include "script.h"
#include "pit.h"
#include "sound.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text_window.h"
#include "overworld.h"
#include "event_data.h"
#include "constants/items.h"
#include "constants/field_weather.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "pokemon_icon.h"
#include "region_map.h"
#include "pokedex.h"
#include "title_screen.h"
#include "main_menu.h"
#include "option_menu.h"
#include "mystery_event_menu.h"
#include "mystery_gift_menu.h"
#include "link.h"
#include "naming_screen.h"
#include "random.h"
#include "trainer_pokemon_sprites.h"
#include "pokemon.h"

/*
 * 
 */
 
//==========DEFINES==========//
struct OutfitsMenuResources
{
    MainCallback savedCallback;     // determines callback to run when we exit. e.g. where do we want to go after closing the menu
    u8 gfxLoadState;
    u16 iconBoxSpriteIds[6];
    u16 iconMonSpriteIds[6];
    u16 mugshotSpriteId[8];
    u8 sSelectedOption;
    u8 returnMode;
    u8 avatarPage;
    u16 currentSpeciesIndex;
};

enum WindowIds
{
    WINDOW_HEADER,
    WINDOW_MIDDLE,
};

enum {
    STATE_BRENDAN,
    STATE_MAY,
    STATE_RED,
    STATE_LEAF,

    STATE_LUCAS,
    STATE_DAWN,
    STATE_ETHAN,
    STATE_LYRA,

    STATE_STEVEN,
    STATE_CYNTHIA,
    STATE_OAK,
    STATE_PHOEBE
};

enum Colors
{
    FONT_BLACK,
    FONT_WHITE,
    FONT_RED,
    FONT_BLUE,
};

enum
{
    HAS_NO_SAVED_GAME,  //NEW GAME, OPTION
    HAS_SAVED_GAME,     //CONTINUE, NEW GAME, OPTION
    HAS_MYSTERY_GIFT,   //CONTINUE, NEW GAME, MYSTERY GIFT, OPTION
    HAS_MYSTERY_EVENTS, //CONTINUE, NEW GAME, MYSTERY GIFT, MYSTERY EVENTS, OPTION
};

#define try_free(ptr) ({        \
    void ** ptr__ = (void **)&(ptr);   \
    if (*ptr__ != NULL)                \
        Free(*ptr__);                  \
})

//==========EWRAM==========//
static EWRAM_DATA struct OutfitsMenuResources *sOutfitsMenuDataPtr = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;
static EWRAM_DATA u8 sSelectedOption = {0};
static EWRAM_DATA u8 menuType = {0};

//==========STATIC=DEFINES==========//
static void OutfitsMenu_RunSetup(void);
static bool8 OutfitsMenu_DoGfxSetup(void);
static bool8 OutfitsMenu_InitBgs(void);
static void OutfitsMenu_FadeAndBail(void);
static bool8 OutfitsMenu_LoadGraphics(void);
static void OutfitsMenu_InitWindows(void);
static void PrintToWindow();
static void PrintToHeader();
static void Task_OutfitsMenuWaitFadeIn(u8 taskId);
static void Task_OutfitsMenuMain(u8 taskId);
static void OutfitsMenu_InitializeGPUWindows(void);

static void CreateMugshot();
static void DestroyMugshot();
static void CreateMugshotsPage1();
static void CreateMugshotsPage2();
void CreatePokemonSprite();
static void CreateIconShadow();
static void DestroyIconShadow();
static u32 GetHPEggCyclePercent(u32 partyIndex);
static void CreatePartyMonIcons();
static void DestroyMonIcons();
static void SetPokemonScreenHWindows(void);


//==========Background and Window Data==========//
static const struct BgTemplate sOutfitsMenuBgTemplates[] =
{
    {
        .bg = 0,                // Text Background
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 0
    }, 
    {
        .bg = 1,                // Main Background
        .charBaseIndex = 3,
        .mapBaseIndex = 30,
        .priority = 2
    },
    {
        .bg = 2,                // Scroll Background
        .charBaseIndex = 2,
        .mapBaseIndex = 28,
        .priority = 2
    }
};

static const struct WindowTemplate sOutfitsMenuWindowTemplates[] = 
{
    [WINDOW_HEADER] = // Prints the Map and Playtime
    {
        .bg = 0,            // which bg to print text on
        .tilemapLeft = 0,  // position from left (per 8 pixels)
        .tilemapTop = 0,    // position from top (per 8 pixels)
        .width = 30,        // width (per 8 pixels)
        .height = 2,        // height (per 8 pixels)
        .paletteNum = 15,    // palette index to use for text
        .baseBlock = 1,     // tile start in VRAM
    },

    [WINDOW_MIDDLE] = // Prints the name, dex number, and badges
    {
        .bg = 0,            // which bg to print text on
        .tilemapLeft = 0,  // position from left (per 8 pixels)
        .tilemapTop = 2,    // position from top (per 8 pixels)
        .width = 30,        // width (per 8 pixels)
        .height = 18,        // height (per 8 pixels)
        .paletteNum = 15,    // palette index to use for text
        .baseBlock = 1 + (30 * 2),     // tile start in VRAM
    },
    DUMMY_WIN_TEMPLATE
};

//  Positions of Hardware/GPU Windows
//       that highlight and hide sections of the Bg
struct HighlightWindowCoords {
    u8 left;
    u8 right;
};

struct HWWindowPosition {
    struct HighlightWindowCoords winh;
    struct HighlightWindowCoords winv;
};

static const struct HWWindowPosition HWinCoords[12] = 
{
    [STATE_BRENDAN]        = {{0, 60},    {0, 56 + 16 + 12},},
    [STATE_MAY]            = {{60, 120},   {0, 56 + 16 + 12},},
    [STATE_RED]            = {{120, 180}, {0, 56 + 16 + 12},},
    [STATE_LEAF]           = {{180, 240},   {0, 56 + 16 + 12},},

    [STATE_LUCAS]           = {{0, 60},      {56 + 16 + 12, 107 + 48 + 16},},
    [STATE_DAWN]            = {{60, 120},    {56 + 16 + 12, 107 + 48 + 16},},
    [STATE_ETHAN]           = {{120, 180},   {56 + 16 + 12, 107 + 48 + 16},},
    [STATE_LYRA]            = {{180, 240},   {56 + 16 + 12, 107 + 48 + 16},},

    [STATE_STEVEN]            = {{0, 240},   {0, 160},},
};


//
//  Graphic and Tilemap Pointers for Bgs and Mughsots
//
static const u32 sMainBgTiles[] = INCBIN_U32("graphics/ui_outfits/main_tiles.4bpp.lz");
static const u32 sMainBgTilemap[] = INCBIN_U32("graphics/ui_outfits/main_tiles.bin.lz");
static const u16 sMainBgPalette[] = INCBIN_U16("graphics/ui_outfits/main_tiles.gbapal");

static const u32 sMainBgTilesFem[] = INCBIN_U32("graphics/ui_outfits/main_tiles_fem.4bpp.lz");
static const u32 sMainBgTilemapFem[] = INCBIN_U32("graphics/ui_outfits/main_tiles_fem.bin.lz");
static const u16 sMainBgPaletteFem[] = INCBIN_U16("graphics/ui_outfits/main_tiles_fem.gbapal");

static const u32 sScrollBgTiles[] = INCBIN_U32("graphics/ui_outfits/scroll_tiles.4bpp.lz");
static const u32 sScrollBgTilemap[] = INCBIN_U32("graphics/ui_outfits/scroll_tiles.bin.lz");
static const u16 sScrollBgPalette[] = INCBIN_U16("graphics/ui_outfits/scroll_tiles.gbapal");

static const u16 sIconBox_Pal[] = INCBIN_U16("graphics/ui_outfits/icon_shadow.gbapal");
static const u32 sIconBox_Gfx[] = INCBIN_U32("graphics/ui_outfits/icon_shadow.4bpp.lz");

static const u16 sIconBox_PalFem[] = INCBIN_U16("graphics/ui_outfits/icon_shadow_fem.gbapal");
static const u32 sIconBox_GfxFem[] = INCBIN_U32("graphics/ui_outfits/icon_shadow_fem.4bpp.lz");

static const u16 sBrendanMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/brendan_mugshot.gbapal");
static const u32 sBrendanMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/brendan_mugshot.4bpp.lz");
static const u16 sMayMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/may_mugshot.gbapal");
static const u32 sMayMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/may_mugshot.4bpp.lz");

static const u16 sRedMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/red_mugshot.gbapal");
static const u32 sRedMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/red_mugshot.4bpp.lz");
static const u16 sLeafMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/leaf_mugshot.gbapal");
static const u32 sLeafMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/leaf_mugshot.4bpp.lz");

static const u16 sLucasMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/lucas_mugshot.gbapal");
static const u32 sLucasMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/lucas_mugshot.4bpp.lz");
static const u16 sDawnMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/dawn_mugshot.gbapal");
static const u32 sDawnMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/dawn_mugshot.4bpp.lz");

static const u16 sStevenMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/steven.gbapal");
static const u32 sStevenMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/steven.4bpp.lz");
static const u16 sCynthiaMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/cynthia.gbapal");
static const u32 sCynthiaMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/cynthia.4bpp.lz");

static const u16 sOakMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/oak.gbapal");
static const u32 sOakMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/oak.4bpp.lz");
static const u16 sPhoebeMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/phoebe.gbapal");
static const u32 sPhoebeMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/phoebe.4bpp.lz");

static const u16 sEthanMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/ethan.gbapal");
static const u32 sEthanMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/ethan.4bpp.lz");
static const u16 sLyraMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/lyra.gbapal");
static const u32 sLyraMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/lyra.4bpp.lz");

static const u16 sNateMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/nate_mugshot.gbapal");
static const u32 sNateMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/nate_mugshot.4bpp.lz");
static const u16 sRosaMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/rosa_mugshot.gbapal");
static const u32 sRosaMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/rosa_mugshot.4bpp.lz");

static const u16 sWallyMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/wally_mugshot.gbapal");
static const u32 sWallyMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/wally_mugshot.4bpp.lz");
static const u16 sLillieMugshot_Pal[] = INCBIN_U16("graphics/ui_outfits/lillie_mugshot.gbapal");
static const u32 sLillieMugshot_Gfx[] = INCBIN_U32("graphics/ui_outfits/lillie_mugshot.4bpp.lz");

//
//  Sprite Data for Mugshots and Icon Shadows 
//
#define TAG_MUGSHOT_BRENDAN 30012
#define TAG_MUGSHOT_MAY 30013
#define TAG_MUGSHOT_RED 30014
#define TAG_MUGSHOT_LEAF 30015

#define TAG_MUGSHOT_LUCAS 30016
#define TAG_MUGSHOT_DAWN 30017
#define TAG_MUGSHOT_ETHAN 30018
#define TAG_MUGSHOT_LYRA 30019

#define TAG_MUGSHOT_STEVEN 30020
#define TAG_MUGSHOT_CYNTHIA 30021
#define TAG_MUGSHOT_OAK 30022
#define TAG_MUGSHOT_PHOEBE 30023

#define TAG_MUGSHOT_NATE 30024
#define TAG_MUGSHOT_ROSA 30025
#define TAG_MUGSHOT_WALLY 30026
#define TAG_MUGSHOT_LILLIE 30027

#define TAG_ICON_BOX 30006
static const struct OamData sOamData_Mugshot =
{
    .size = SPRITE_SIZE(64x64),
    .shape = SPRITE_SHAPE(64x64),
    .priority = 1,
};

static const union AnimCmd sSpriteAnim_Mugshot[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_Mugshot[] =
{
    sSpriteAnim_Mugshot,
};

static const struct CompressedSpriteSheet sSpriteSheet_BrendanMugshot =
{
    .data = sBrendanMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_BRENDAN,
};

static const struct SpritePalette sSpritePal_BrendanMugshot =
{
    .data = sBrendanMugshot_Pal,
    .tag = TAG_MUGSHOT_BRENDAN,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotBrendan =
{
    .tileTag = TAG_MUGSHOT_BRENDAN,
    .paletteTag = TAG_MUGSHOT_BRENDAN,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_MayMugshot =
{
    .data = sMayMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_MAY,
};

static const struct SpritePalette sSpritePal_MayMugshot =
{
    .data = sMayMugshot_Pal,
    .tag = TAG_MUGSHOT_MAY,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotMay =
{
    .tileTag = TAG_MUGSHOT_MAY,
    .paletteTag = TAG_MUGSHOT_MAY,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_RedMugshot =
{
    .data = sRedMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_RED,
};

static const struct SpritePalette sSpritePal_RedMugshot =
{
    .data = sRedMugshot_Pal,
    .tag = TAG_MUGSHOT_RED,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotRed =
{
    .tileTag = TAG_MUGSHOT_RED,
    .paletteTag = TAG_MUGSHOT_RED,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_LeafMugshot =
{
    .data = sLeafMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_LEAF,
};

static const struct SpritePalette sSpritePal_LeafMugshot =
{
    .data = sLeafMugshot_Pal,
    .tag = TAG_MUGSHOT_LEAF,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotLeaf =
{
    .tileTag = TAG_MUGSHOT_LEAF,
    .paletteTag = TAG_MUGSHOT_LEAF,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_LucasMugshot =
{
    .data = sLucasMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_LUCAS,
};

static const struct SpritePalette sSpritePal_LucasMugshot =
{
    .data = sLucasMugshot_Pal,
    .tag = TAG_MUGSHOT_LUCAS,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotLucas =
{
    .tileTag = TAG_MUGSHOT_LUCAS,
    .paletteTag = TAG_MUGSHOT_LUCAS,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_DawnMugshot =
{
    .data = sDawnMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_DAWN
};

static const struct SpritePalette sSpritePal_DawnMugshot =
{
    .data = sDawnMugshot_Pal,
    .tag = TAG_MUGSHOT_DAWN,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotDawn =
{
    .tileTag = TAG_MUGSHOT_DAWN,
    .paletteTag = TAG_MUGSHOT_DAWN,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_StevenMugshot =
{
    .data = sStevenMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_STEVEN,
};

static const struct SpritePalette sSpritePal_StevenMugshot =
{
    .data = sStevenMugshot_Pal,
    .tag = TAG_MUGSHOT_STEVEN,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotSteven =
{
    .tileTag = TAG_MUGSHOT_STEVEN,
    .paletteTag = TAG_MUGSHOT_STEVEN,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_CynthiaMugshot =
{
    .data = sCynthiaMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_CYNTHIA,
};

static const struct SpritePalette sSpritePal_CynthiaMugshot =
{
    .data = sCynthiaMugshot_Pal,
    .tag = TAG_MUGSHOT_CYNTHIA,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotCynthia =
{
    .tileTag = TAG_MUGSHOT_CYNTHIA,
    .paletteTag = TAG_MUGSHOT_CYNTHIA,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_OakMugshot =
{
    .data = sOakMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_OAK,
};

static const struct SpritePalette sSpritePal_OakMugshot =
{
    .data = sOakMugshot_Pal,
    .tag = TAG_MUGSHOT_OAK,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotOak =
{
    .tileTag = TAG_MUGSHOT_OAK,
    .paletteTag = TAG_MUGSHOT_OAK,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_PhoebeMugshot =
{
    .data = sPhoebeMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_PHOEBE,
};

static const struct SpritePalette sSpritePal_PhoebeMugshot =
{
    .data = sPhoebeMugshot_Pal,
    .tag = TAG_MUGSHOT_PHOEBE,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotPhoebe =
{
    .tileTag = TAG_MUGSHOT_PHOEBE,
    .paletteTag = TAG_MUGSHOT_PHOEBE,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_EthanMugshot =
{
    .data = sEthanMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_ETHAN,
};

static const struct SpritePalette sSpritePal_EthanMugshot =
{
    .data = sEthanMugshot_Pal,
    .tag = TAG_MUGSHOT_ETHAN,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotEthan =
{
    .tileTag = TAG_MUGSHOT_ETHAN,
    .paletteTag = TAG_MUGSHOT_ETHAN,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_LyraMugshot =
{
    .data = sLyraMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_LYRA,
};

static const struct SpritePalette sSpritePal_LyraMugshot =
{
    .data = sLyraMugshot_Pal,
    .tag = TAG_MUGSHOT_LYRA,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotLyra =
{
    .tileTag = TAG_MUGSHOT_LYRA,
    .paletteTag = TAG_MUGSHOT_LYRA,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};



static const struct CompressedSpriteSheet sSpriteSheet_NateMugshot =
{
    .data = sNateMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_NATE,
};

static const struct SpritePalette sSpritePal_NateMugshot =
{
    .data = sNateMugshot_Pal,
    .tag = TAG_MUGSHOT_NATE,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotNate =
{
    .tileTag = TAG_MUGSHOT_NATE,
    .paletteTag = TAG_MUGSHOT_NATE,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_RosaMugshot =
{
    .data = sRosaMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_ROSA,
};

static const struct SpritePalette sSpritePal_RosaMugshot =
{
    .data = sRosaMugshot_Pal,
    .tag = TAG_MUGSHOT_ROSA,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotRosa =
{
    .tileTag = TAG_MUGSHOT_ROSA,
    .paletteTag = TAG_MUGSHOT_ROSA,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_WallyMugshot =
{
    .data = sWallyMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_WALLY,
};

static const struct SpritePalette sSpritePal_WallyMugshot =
{
    .data = sWallyMugshot_Pal,
    .tag = TAG_MUGSHOT_WALLY,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotWally =
{
    .tileTag = TAG_MUGSHOT_WALLY,
    .paletteTag = TAG_MUGSHOT_WALLY,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct CompressedSpriteSheet sSpriteSheet_LillieMugshot =
{
    .data = sLillieMugshot_Gfx,
    .size = 64*64*1/2,
    .tag = TAG_MUGSHOT_LILLIE,
};

static const struct SpritePalette sSpritePal_LillieMugshot =
{
    .data = sLillieMugshot_Pal,
    .tag = TAG_MUGSHOT_LILLIE,
};

static const struct SpriteTemplate sSpriteTemplate_MugshotLillie =
{
    .tileTag = TAG_MUGSHOT_LILLIE,
    .paletteTag = TAG_MUGSHOT_LILLIE,
    .oam = &sOamData_Mugshot,
    .anims = sSpriteAnimTable_Mugshot,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};




// Icon Box
static const struct OamData sOamData_IconBox =
{
    .size = SPRITE_SIZE(32x32),
    .shape = SPRITE_SHAPE(32x32),
    .priority = 1,
};

static const struct CompressedSpriteSheet sSpriteSheet_IconBox =
{
    .data = sIconBox_Gfx,
    .size = 32*32*1/2,
    .tag = TAG_ICON_BOX,
};

static const struct CompressedSpriteSheet sSpriteSheet_IconBoxFem =
{
    .data = sIconBox_GfxFem,
    .size = 32*32*1/2,
    .tag = TAG_ICON_BOX,
};

static const struct SpritePalette sSpritePal_IconBox =
{
    .data = sIconBox_Pal,
    .tag = TAG_ICON_BOX
};

static const struct SpritePalette sSpritePal_IconBoxFem =
{
    .data = sIconBox_PalFem,
    .tag = TAG_ICON_BOX
};

static const union AnimCmd sSpriteAnim_IconBox0[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_IconBox[] =
{
    sSpriteAnim_IconBox0,
};

static const struct SpriteTemplate sSpriteTemplate_IconBox =
{
    .tileTag = TAG_ICON_BOX,
    .paletteTag = TAG_ICON_BOX,
    .oam = &sOamData_IconBox,
    .anims = sSpriteAnimTable_IconBox,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

//==========UI Initialization from Ghoulslash's UI Shell Branch==========//
void Task_OpenOutfitsMenu(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    if (!gPaletteFade.active)
    {   
        CleanupOverworldWindowsAndTilemaps();
        OutfitsMenu_Init(CB2_InitTitleScreen, OUTFITS_NORMAL_AVATAR_MENU); // if need to bail go to title screen
        DestroyTask(taskId);
    }
}

//
//  Setup Menu Functions
//
void OutfitsMenu_Init(MainCallback callback, u8 mode)
{
    u32 i = 0;
    if ((sOutfitsMenuDataPtr = AllocZeroed(sizeof(struct OutfitsMenuResources))) == NULL)
    {
        SetMainCallback2(callback);
        return;
    }

    sOutfitsMenuDataPtr->returnMode = mode;

    sOutfitsMenuDataPtr->gfxLoadState = 0;
    sOutfitsMenuDataPtr->savedCallback = callback;

    sSelectedOption = gSaveBlock2Ptr->playerGfxType % 8;

    if (gSaveBlock2Ptr->playerGfxType > 15)
    {
        sOutfitsMenuDataPtr->avatarPage = 2;
    }
    else if (gSaveBlock2Ptr->playerGfxType > 7)
    {
        sOutfitsMenuDataPtr->avatarPage = 1;
    }
    else
    {
        sOutfitsMenuDataPtr->avatarPage = 0;
    }

    for(i = 0; i < 8; i++)
    {
        sOutfitsMenuDataPtr->mugshotSpriteId[i] = SPRITE_NONE;
    }
    
    SetMainCallback2(OutfitsMenu_RunSetup);
}

static void OutfitsMenu_RunSetup(void)
{
    while (1)
    {
        if (OutfitsMenu_DoGfxSetup() == TRUE)
            break;
    }
}

static void OutfitsMenu_MainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void OutfitsMenu_VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    ChangeBgY(2, 128, BG_COORD_SUB); // This controls the scrolling of the scroll bg, remove it to stop scrolling
}

//
//  Quit Out Functions
//
static void OutfitsMenu_FreeResources(void)
{
    DestroyMugshot();
    DestroyIconShadow();
    DestroyMonIcons();
    try_free(sOutfitsMenuDataPtr);
    try_free(sBg1TilemapBuffer);
    try_free(sBg2TilemapBuffer);
    FreeAllWindowBuffers();
    DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
}

static void Task_OutfitsMenuWaitFadeAndBail(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sOutfitsMenuDataPtr->savedCallback);
        OutfitsMenu_FreeResources();
        DestroyTask(taskId);
    }
}

static void OutfitsMenu_FadeAndBail(void)
{
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_OutfitsMenuWaitFadeAndBail, 0);
    SetVBlankCallback(OutfitsMenu_VBlankCB);
    SetMainCallback2(OutfitsMenu_MainCB);
}

static void Task_OutfitsMenuWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_OutfitsMenuMain;
}

static void Task_OutfitsMenuTurnOff(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WIN1H, 0);
        SetGpuReg(REG_OFFSET_WIN1V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        if (sOutfitsMenuDataPtr->returnMode == OUTFITS_NORMAL_AVATAR_MENU)
        {
            SetMainCallback2(sOutfitsMenuDataPtr->savedCallback);
        }
        else
        {
            gMain.savedCallback = CB2_ReinitMainMenu;
            NewGameBirchSpeech_SetDefaultPlayerName(Random() % 19);    
            if(sOutfitsMenuDataPtr->avatarPage == 2)
            {
                DoNamingScreen(NAMING_SCREEN_PLAYER_IS_POKEMON, gSaveBlock2Ptr->playerName, gSaveBlock2Ptr->pokemonAvatarSpecies, 0, 0, CB2_InitOptionMenu);  
            }
            else
            {
                DoNamingScreen(NAMING_SCREEN_PLAYER, gSaveBlock2Ptr->playerName, gSaveBlock2Ptr->playerGender, 0, 0, CB2_InitOptionMenu);  
            }
              
        }
        
        OutfitsMenu_FreeResources();
        DestroyTask(taskId);
    }
}


//
//  Load Graphics Functions
//
static bool8 OutfitsMenu_DoGfxSetup(void)
{
    switch (gMain.state)
    {
    case 0:
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000)
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        ResetVramOamAndBgCntRegs();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        OutfitsMenu_InitializeGPUWindows();
        gMain.state++;
        break;
    case 2:
        if (OutfitsMenu_InitBgs())
        {
            sOutfitsMenuDataPtr->gfxLoadState = 0;
            gMain.state++;
        }
        else
        {
            OutfitsMenu_FadeAndBail();
            return TRUE;
        }
        break;
    case 3:
        if (OutfitsMenu_LoadGraphics() == TRUE)
            gMain.state++;
        break;
    case 4:
        LoadMessageBoxAndBorderGfx();
        OutfitsMenu_InitWindows();
        gMain.state++;
        break;
    case 5: // Here is where the sprites are drawn and text is printed
        sOutfitsMenuDataPtr->currentSpeciesIndex = GetIndexOfSpeciesInValidSpeciesArray(gSaveBlock2Ptr->pokemonAvatarSpecies);
        if(sOutfitsMenuDataPtr->avatarPage == 0)
            CreateMugshotsPage1();
        if(sOutfitsMenuDataPtr->avatarPage == 1)
            CreateMugshotsPage2();
        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            SetPokemonScreenHWindows();
            CreatePokemonSprite();
        }   
        PrintToWindow();
        PrintToHeader();
        CreateTask(Task_OutfitsMenuWaitFadeIn, 0);
        BlendPalettes(0xFFFFFFFF, 16, RGB_BLACK);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    default:
        SetVBlankCallback(OutfitsMenu_VBlankCB);
        SetMainCallback2(OutfitsMenu_MainCB);
        return TRUE;
    }
    return FALSE;
}

static bool8 OutfitsMenu_InitBgs(void)
{
    ResetAllBgsCoordinates();
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sOutfitsMenuBgTemplates, NELEMS(sOutfitsMenuBgTemplates));

    sBg1TilemapBuffer = Alloc(0x800);
    if (sBg1TilemapBuffer == NULL)
        return FALSE;
    memset(sBg1TilemapBuffer, 0, 0x800);
    SetBgTilemapBuffer(1, sBg1TilemapBuffer);
    ScheduleBgCopyTilemapToVram(1);

    sBg2TilemapBuffer = Alloc(0x800);
    if (sBg2TilemapBuffer == NULL)
        return FALSE;
    memset(sBg2TilemapBuffer, 0, 0x800);
    SetBgTilemapBuffer(2, sBg2TilemapBuffer);
    ScheduleBgCopyTilemapToVram(2);

    ShowBg(0);
    ShowBg(2);
    return TRUE;
}

static void OutfitsMenu_InitializeGPUWindows(void) // This function creates the windows that highlight an option and cover mystery options when not enabled
{
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN1_ON | DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP); // Turn on Windows 0 and 1 and Enable Sprites
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(HWinCoords[sSelectedOption].winh.left, HWinCoords[sSelectedOption].winh.right));  // Set Window 0 width/height Which defines the not darkened space
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(HWinCoords[sSelectedOption].winv.left, HWinCoords[sSelectedOption].winv.right));
    switch(menuType)
    {
            case HAS_SAVED_GAME:    // The three Window 1 states either block out the mystery buttons both, just the mystery event, or nothing. 
                SetGpuReg(REG_OFFSET_WIN1H,  WIN_RANGE(HWinCoords[STATE_DAWN].winh.left, HWinCoords[STATE_DAWN].winh.right));
                SetGpuReg(REG_OFFSET_WIN1V,  WIN_RANGE(HWinCoords[STATE_DAWN].winv.left, HWinCoords[STATE_DAWN].winv.right));
                break;   
            case HAS_MYSTERY_GIFT:
                SetGpuReg(REG_OFFSET_WIN1H,  WIN_RANGE(HWinCoords[STATE_LUCAS].winh.left, HWinCoords[STATE_LUCAS].winh.right));
                SetGpuReg(REG_OFFSET_WIN1V,  WIN_RANGE(HWinCoords[STATE_LUCAS].winv.left, HWinCoords[STATE_LUCAS].winv.right));
                break;
        }

    SetGpuReg(REG_OFFSET_WININ, (WININ_WIN1_BG0 | WININ_WIN1_BG2) | (WININ_WIN0_BG_ALL | WININ_WIN0_OBJ)); // Set Win 0 Active everywhere, Win 1 active on everything except bg 1 
    SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_ALL);                                                                     // where the main tiles are so the window hides whats behind it
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_BG1 | BLDCNT_TGT1_OBJ);   // Set Darken Effect on things not in the window on bg 0, 1, and sprite layer
    SetGpuReg(REG_OFFSET_BLDY, 7);  // Set Level of Darken effect, can be changed 0-16
}

static void MoveHWindowsWithInput(void) // Update GPU windows after selection is changed
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(HWinCoords[sSelectedOption].winh.left, HWinCoords[sSelectedOption].winh.right));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(HWinCoords[sSelectedOption].winv.left, HWinCoords[sSelectedOption].winv.right));
}

static void SetPokemonScreenHWindows(void) // Update GPU windows after selection is changed
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(HWinCoords[STATE_STEVEN].winh.left, HWinCoords[STATE_STEVEN].winh.right));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(HWinCoords[STATE_STEVEN].winv.left, HWinCoords[STATE_STEVEN].winv.right));
}

void LoadSecondPageSpriteGraphics(void)
{
    LoadCompressedSpriteSheet(&sSpriteSheet_StevenMugshot);
    LoadSpritePalette(&sSpritePal_StevenMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_CynthiaMugshot);
    LoadSpritePalette(&sSpritePal_CynthiaMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_PhoebeMugshot);
    LoadSpritePalette(&sSpritePal_PhoebeMugshot);       
    LoadCompressedSpriteSheet(&sSpriteSheet_OakMugshot);
    LoadSpritePalette(&sSpritePal_OakMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_NateMugshot);
    LoadSpritePalette(&sSpritePal_NateMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_RosaMugshot);
    LoadSpritePalette(&sSpritePal_RosaMugshot);       
    LoadCompressedSpriteSheet(&sSpriteSheet_WallyMugshot);
    LoadSpritePalette(&sSpritePal_WallyMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_LillieMugshot);
    LoadSpritePalette(&sSpritePal_LillieMugshot);
}

void LoadFirstPageSpriteGraphics(void)
{
    LoadCompressedSpriteSheet(&sSpriteSheet_BrendanMugshot);
    LoadSpritePalette(&sSpritePal_BrendanMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_MayMugshot);
    LoadSpritePalette(&sSpritePal_MayMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_RedMugshot);
    LoadSpritePalette(&sSpritePal_RedMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_LeafMugshot);
    LoadSpritePalette(&sSpritePal_LeafMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_LucasMugshot);
    LoadSpritePalette(&sSpritePal_LucasMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_DawnMugshot);
    LoadSpritePalette(&sSpritePal_DawnMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_EthanMugshot);
    LoadSpritePalette(&sSpritePal_EthanMugshot);
    LoadCompressedSpriteSheet(&sSpriteSheet_LyraMugshot);
    LoadSpritePalette(&sSpritePal_LyraMugshot);
}


static bool8 OutfitsMenu_LoadGraphics(void) // Load all the tilesets, tilemaps, spritesheets, and palettes
{
    switch (sOutfitsMenuDataPtr->gfxLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        sOutfitsMenuDataPtr->gfxLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            sOutfitsMenuDataPtr->gfxLoadState++;
        }
        break;
    case 2:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(2, sScrollBgTiles, 0, 0, 0);
        sOutfitsMenuDataPtr->gfxLoadState++;
        break;
    case 3:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LZDecompressWram(sScrollBgTilemap, sBg2TilemapBuffer);
            sOutfitsMenuDataPtr->gfxLoadState++;
        }
        break;
    case 4:
    {
        if(sOutfitsMenuDataPtr->avatarPage == 0)
            LoadFirstPageSpriteGraphics();
        if(sOutfitsMenuDataPtr->avatarPage == 1)
            LoadSecondPageSpriteGraphics();

        LoadPalette(sScrollBgPalette, 16, 32);
    }
        sOutfitsMenuDataPtr->gfxLoadState++;
        break;
    default:
        sOutfitsMenuDataPtr->gfxLoadState = 0;
        return TRUE;
    }
    return FALSE;
}

static void OutfitsMenu_InitWindows(void) // Init Text Windows
{
    InitWindows(sOutfitsMenuWindowTemplates);
    DeactivateAllTextPrinters();
    ScheduleBgCopyTilemapToVram(0);
    
    FillWindowPixelBuffer(WINDOW_HEADER, PIXEL_FILL(1));
    PutWindowTilemap(WINDOW_HEADER);
    CopyWindowToVram(WINDOW_HEADER, 3);

    FillWindowPixelBuffer(WINDOW_MIDDLE, 0);
    PutWindowTilemap(WINDOW_MIDDLE);
    CopyWindowToVram(WINDOW_MIDDLE, 3);
}


/*    Sprite Creation Functions     */

//
//      Mugshot Functions
//
#define MUGSHOT_X_1 30
#define MUGSHOT_X_2 90
#define MUGSHOT_X_3 150
#define MUGSHOT_X_4 210

#define MUGSHOT_Y_1 32 + 8 + 16
#define MUGSHOT_Y_2 86 + 16 + 12
#define MUGSHOT_Y_3 138


static void CreateMugshotsPage1()
{
    sOutfitsMenuDataPtr->mugshotSpriteId[0] = CreateSprite(&sSpriteTemplate_MugshotBrendan, MUGSHOT_X_1, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[1] = CreateSprite(&sSpriteTemplate_MugshotMay,     MUGSHOT_X_2, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[2] = CreateSprite(&sSpriteTemplate_MugshotRed,     MUGSHOT_X_3, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[3] = CreateSprite(&sSpriteTemplate_MugshotLeaf,    MUGSHOT_X_4, MUGSHOT_Y_1, 1);

    sOutfitsMenuDataPtr->mugshotSpriteId[4] = CreateSprite(&sSpriteTemplate_MugshotLucas, MUGSHOT_X_1, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[5] = CreateSprite(&sSpriteTemplate_MugshotDawn,  MUGSHOT_X_2, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[6] = CreateSprite(&sSpriteTemplate_MugshotEthan, MUGSHOT_X_3, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[7] = CreateSprite(&sSpriteTemplate_MugshotLyra,  MUGSHOT_X_4, MUGSHOT_Y_2, 1);
    return;
}


static void CreateMugshotsPage2()
{
    sOutfitsMenuDataPtr->mugshotSpriteId[0] = CreateSprite(&sSpriteTemplate_MugshotSteven,  MUGSHOT_X_1, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[1] = CreateSprite(&sSpriteTemplate_MugshotCynthia, MUGSHOT_X_2, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[2] = CreateSprite(&sSpriteTemplate_MugshotOak,     MUGSHOT_X_3, MUGSHOT_Y_1, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[3] = CreateSprite(&sSpriteTemplate_MugshotPhoebe,  MUGSHOT_X_4, MUGSHOT_Y_1, 1);

    sOutfitsMenuDataPtr->mugshotSpriteId[4] = CreateSprite(&sSpriteTemplate_MugshotNate, MUGSHOT_X_1, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[5] = CreateSprite(&sSpriteTemplate_MugshotRosa,  MUGSHOT_X_2, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[6] = CreateSprite(&sSpriteTemplate_MugshotWally, MUGSHOT_X_3, MUGSHOT_Y_2, 1);
    sOutfitsMenuDataPtr->mugshotSpriteId[7] = CreateSprite(&sSpriteTemplate_MugshotLillie,  MUGSHOT_X_4, MUGSHOT_Y_2, 1);
    return;
}

static void DestroyMugshot()
{
    if (sOutfitsMenuDataPtr->avatarPage == 2)
    {
        FreeAndDestroyMonPicSprite(sOutfitsMenuDataPtr->mugshotSpriteId[0]);
        return;
    }
        
    for(u8 i = 0; i < 8; i++)
    {
        if(sOutfitsMenuDataPtr->mugshotSpriteId[i] != SPRITE_NONE)
        {
            DestroySpriteAndFreeResources(&gSprites[sOutfitsMenuDataPtr->mugshotSpriteId[i]]);
            sOutfitsMenuDataPtr->mugshotSpriteId[i] = SPRITE_NONE;
        }
    }
}

void CreatePokemonSprite()
{
    sOutfitsMenuDataPtr->mugshotSpriteId[0] = CreateMonPicSprite(AccessValidSpeciesArrayIndex(sOutfitsMenuDataPtr->currentSpeciesIndex), 
                    FALSE, 
                    0, 
                    TRUE, 
                    120,
                    80,
                    15, 
                    TAG_NONE);
}

//
//  Create Mon Icon and Shadow Sprites
//
static void CreateIconShadow()
{

}

static void DestroyIconShadow()
{

}

static void CreatePartyMonIcons()
{

}

static void DestroyMonIcons()
{

}


//
//  Print The Text For Dex Num, Badges, Name, Playtime, Location
//
static const u8 sText_Avatar_Brendan[] = _("Brendan");
static const u8 sText_Avatar_May[] = _("May");
static const u8 sText_Avatar_Red[] = _("Red");
static const u8 sText_Avatar_Leaf[] = _("Leaf");

static const u8 sText_Avatar_Lucas[] = _("Lucas");
static const u8 sText_Avatar_Dawn[] = _("Dawn");
static const u8 sText_Avatar_Ethan[] = _("Ethan");
static const u8 sText_Avatar_Lyra[] = _("Lyra");

static const u8 sText_Avatar_Steven[] = _("Steven");
static const u8 sText_Avatar_Cynthia[] = _("Cynthia");
static const u8 sText_Avatar_Oak[] = _("Oak");
static const u8 sText_Avatar_Phoebe[] = _("Phoebe");

static const u8 sText_Avatar_Nate[] = _("Nate");
static const u8 sText_Avatar_Rosa[] = _("Rosa");
static const u8 sText_Avatar_Wally[] = _("Wally");
static const u8 sText_Avatar_Lillie[] = _("Lillie");

static const u8 sText_Avatar_Pokemon[] = _("Pokemon");

static void PrintToWindow()
{
    const u8 colors[3] = {0,  2,  3}; 
    const u8 *avatarName = sText_Avatar_Pokemon;

    u16 x = 92;

    switch(sSelectedOption)
    {
        case 0:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Brendan;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Steven;
            break;
        case 1:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_May;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Cynthia;
            break;
        case 2:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Red;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Oak;
            break;
        case 3:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Leaf;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Phoebe;
            break;
        case 4:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Lucas;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Nate;
            break;
        case 5:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Dawn;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Rosa;
            break;
        case 6:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Ethan;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Wally;
            break;
        case 7:
            if (sOutfitsMenuDataPtr->avatarPage == 0)
                avatarName = sText_Avatar_Lyra;
            if (sOutfitsMenuDataPtr->avatarPage == 1)
                avatarName = sText_Avatar_Lillie;
            break;
    }

    if (sOutfitsMenuDataPtr->avatarPage == 2)
    {
        avatarName = GetSpeciesName(AccessValidSpeciesArrayIndex(sOutfitsMenuDataPtr->currentSpeciesIndex));
    }

    FillWindowPixelBuffer(WINDOW_MIDDLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    AddTextPrinterParameterized4(WINDOW_MIDDLE, FONT_NORMAL, x + GetStringCenterAlignXOffset(FONT_NORMAL, avatarName, (6*10)), 140 - 14, 0, 0, colors, TEXT_SKIP_DRAW, avatarName);

    PutWindowTilemap(WINDOW_MIDDLE);
    CopyWindowToVram(WINDOW_MIDDLE, 3);
}

static const u8 sText_Avatar_Choose_Avatar[] = _("Choose an Avatar: ");
static const u8 sText_Avatar_Choose_Avatar_Buttons[] = _("{COLOR WHITE}{DPAD_LEFTRIGHT}");
static const u8 sText_Avatar_ChangePage[]    = _("Change Page: ");
static const u8 sText_Avatar_ChangePage_Buttons[]    = _("{COLOR WHITE}{L_BUTTON} {R_BUTTON}");
static const u8 sLR_ButtonGfx[]      = INCBIN_U8("graphics/ui_menu/r_button2.4bpp");
static const u8 sDPad_ButtonGfx[]         = INCBIN_U8("graphics/ui_menu/dpad_button2.4bpp");

static void PrintToHeader()
{
    const u8 colors[3] = {1,  2,  3}; 
    const u8 colors2[3] = {1,  3,  2}; 
    const u8 *avatarName = sText_Avatar_Pokemon;

    FillWindowPixelBuffer(WINDOW_HEADER, PIXEL_FILL(1));

    AddTextPrinterParameterized4(WINDOW_HEADER, FONT_NORMAL, 4, 0, 0, 0, colors, TEXT_SKIP_DRAW, sText_Avatar_Choose_Avatar);
    BlitBitmapToWindow(WINDOW_HEADER, sDPad_ButtonGfx, 100, (4), 24, 8);

    AddTextPrinterParameterized4(WINDOW_HEADER, FONT_NORMAL, 144, 0, 0, 0, colors, TEXT_SKIP_DRAW, sText_Avatar_ChangePage);
    BlitBitmapToWindow(WINDOW_HEADER, sLR_ButtonGfx, 212, 4, 24, 8);

    PutWindowTilemap(WINDOW_HEADER);
    CopyWindowToVram(WINDOW_HEADER, 3);
}

static void Task_OutfitsMenu_SwapPokemonSprite(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if(data[8] == 0)
    {
        DestroyMugshot();
        FillWindowPixelBuffer(WINDOW_MIDDLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        PutWindowTilemap(WINDOW_MIDDLE);
        CopyWindowToVram(WINDOW_MIDDLE, 3);
        data[8]++;
        return;
    }

    data[8]++;

    if(data[8] > 4)
    {
        CreatePokemonSprite();
        PrintToWindow();
        data[8] = 0;
        gTasks[taskId].func = Task_OutfitsMenuMain;
        return;
    }
}

static void Task_OutfitsMenu_Transition(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if(data[8] == 0)
    {
        FillWindowPixelBuffer(WINDOW_MIDDLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        PutWindowTilemap(WINDOW_MIDDLE);
        CopyWindowToVram(WINDOW_MIDDLE, 3);
        data[8]++;
        return;
    }

    if(data[8] == 1)
    {
        if(sOutfitsMenuDataPtr->avatarPage == 0)
        {
            LoadFirstPageSpriteGraphics();
        }

        if(sOutfitsMenuDataPtr->avatarPage == 1)
        {
            LoadSecondPageSpriteGraphics();
        }

        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            
        }
        data[8]++;
        return;
    }

    if(data[8] == 2)
    {
        if(sOutfitsMenuDataPtr->avatarPage == 0)
        {
            MoveHWindowsWithInput();
            CreateMugshotsPage1();
        }

        if(sOutfitsMenuDataPtr->avatarPage == 1)
        {
            MoveHWindowsWithInput();
            CreateMugshotsPage2();
        }

        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            CreatePokemonSprite();
            SetPokemonScreenHWindows();
        }
        PrintToWindow();
        data[8] = 0;
        gTasks[taskId].func = Task_OutfitsMenuMain;
        return;
    }
}

//
//  Main Input Control Task
//
static void Task_OutfitsMenuMain(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON) || JOY_NEW(START_BUTTON)) 
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        if(sOutfitsMenuDataPtr->avatarPage == 0)
        {
            gSaveBlock2Ptr->playerGfxType = sSelectedOption;
            gSaveBlock2Ptr->playerGender = sSelectedOption % 2;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 1)
        {
            gSaveBlock2Ptr->playerGfxType = sSelectedOption + 8;
            gSaveBlock2Ptr->playerGender = sSelectedOption % 2;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            gSaveBlock2Ptr->pokemonAvatarSpecies = AccessValidSpeciesArrayIndex(sOutfitsMenuDataPtr->currentSpeciesIndex);
            gSaveBlock2Ptr->playerGfxType = AVATAR_POKEMON_CHOICE;
            gSaveBlock2Ptr->playerGender = MALE;
        }
        
        gTasks[taskId].func = Task_OutfitsMenuTurnOff;
    }

    if(JOY_NEW(L_BUTTON))
    {
        if(sOutfitsMenuDataPtr->avatarPage == 0)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 2;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 1)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 0;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 1;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }
    }

    if(JOY_NEW(R_BUTTON))
    {
        if(sOutfitsMenuDataPtr->avatarPage == 0)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 1;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 1)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 2;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }

        if(sOutfitsMenuDataPtr->avatarPage == 2)
        {
            DestroyMugshot();
            sOutfitsMenuDataPtr->avatarPage = 0;
            gTasks[taskId].func = Task_OutfitsMenu_Transition;
            return;
        }
    }

    #define UP_DOWN_SPECIES_JUMP 10
    if(sOutfitsMenuDataPtr->avatarPage == 2)
    {
        if(JOY_NEW(DPAD_DOWN) || JOY_HELD(DPAD_DOWN)) // Handle DPad directions, kinda bad way to do it with each case handled individually but its whatever
        {   
            u16 temp = 0;
            if(sOutfitsMenuDataPtr->currentSpeciesIndex < UP_DOWN_SPECIES_JUMP)
            {
                temp = GetMaxNumberOfSpecies() - (UP_DOWN_SPECIES_JUMP - sOutfitsMenuDataPtr->currentSpeciesIndex);
                sOutfitsMenuDataPtr->currentSpeciesIndex = temp;
            }
            else
            {
                sOutfitsMenuDataPtr->currentSpeciesIndex -= UP_DOWN_SPECIES_JUMP;
            }

            if (sOutfitsMenuDataPtr->currentSpeciesIndex >= GetMaxNumberOfSpecies())
                sOutfitsMenuDataPtr->currentSpeciesIndex = 0;
            PrintToWindow();
            gTasks[taskId].func = Task_OutfitsMenu_SwapPokemonSprite;
            return;
        }

        if(JOY_NEW(DPAD_UP) || JOY_HELD(DPAD_UP))
        {
            sOutfitsMenuDataPtr->currentSpeciesIndex += UP_DOWN_SPECIES_JUMP;
            if (sOutfitsMenuDataPtr->currentSpeciesIndex >= GetMaxNumberOfSpecies())
                sOutfitsMenuDataPtr->currentSpeciesIndex = 0;
            PrintToWindow();
            gTasks[taskId].func = Task_OutfitsMenu_SwapPokemonSprite;
            return;
        }

        if(JOY_NEW(DPAD_LEFT) || JOY_HELD(DPAD_LEFT))
        {
            if(sOutfitsMenuDataPtr->currentSpeciesIndex == 0)
                sOutfitsMenuDataPtr->currentSpeciesIndex = GetMaxNumberOfSpecies() - 1;
            else
                sOutfitsMenuDataPtr->currentSpeciesIndex--;

            if (sOutfitsMenuDataPtr->currentSpeciesIndex >= GetMaxNumberOfSpecies())
                sOutfitsMenuDataPtr->currentSpeciesIndex = 0;
            PrintToWindow();
            gTasks[taskId].func = Task_OutfitsMenu_SwapPokemonSprite;
            return;
        }

        if(JOY_NEW(DPAD_RIGHT) || JOY_HELD(DPAD_RIGHT))
        {
            sOutfitsMenuDataPtr->currentSpeciesIndex++;
            if (sOutfitsMenuDataPtr->currentSpeciesIndex >= GetMaxNumberOfSpecies())
                sOutfitsMenuDataPtr->currentSpeciesIndex = 0;
            PrintToWindow();
            gTasks[taskId].func = Task_OutfitsMenu_SwapPokemonSprite;
            return;
        }
    }
    else
    {
        if(JOY_NEW(DPAD_DOWN)) // Handle DPad directions, kinda bad way to do it with each case handled individually but its whatever
        {
            if(sSelectedOption > STATE_LEAF)
                sSelectedOption -= 4;
            else
                sSelectedOption += 4;
            MoveHWindowsWithInput();
            PrintToWindow();
        }

        if(JOY_NEW(DPAD_UP))
        {
            if(sSelectedOption <= STATE_LEAF)
                sSelectedOption += 4;
            else
                sSelectedOption -= 4;
            MoveHWindowsWithInput();
            PrintToWindow();
        }

        if(JOY_NEW(DPAD_LEFT))
        {
            if((sSelectedOption == STATE_BRENDAN) || (sSelectedOption == STATE_LUCAS))
                sSelectedOption += 3;
            else
                sSelectedOption -= 1;
            MoveHWindowsWithInput();
            PrintToWindow();
        }

        if(JOY_NEW(DPAD_RIGHT))
        {
            if((sSelectedOption == STATE_LEAF) || (sSelectedOption == STATE_LYRA))
                sSelectedOption -= 3;
            else 
                sSelectedOption += 1;
            MoveHWindowsWithInput();
            PrintToWindow();
        }
    }


}
