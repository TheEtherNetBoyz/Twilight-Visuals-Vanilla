#include "menu_scaling.hpp"

#include "hook_api.hpp"
#include "runtime.hpp"

#include "d/d_bright_check.h"
#include "d/d_menu_collect.h"
#include "d/d_menu_save.h"
#include "d/d_name.h"
#include "d/d_file_select.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <array>

namespace twilight_visuals::menu_scaling {
namespace {

struct PaneCache {
    u64 tag;
    f32 origTransX;
    f32 origTransY;
    bool cached;
};

DEFINE_HOOK_SYMBOL("dMenu_Collect2D_c::menuCollectWide", void(dMenu_Collect2D_c*),
                   CollectWide);
DEFINE_HOOK_SYMBOL("dMenu_save_c::menuSaveWide", void(dMenu_save_c*), SaveWide);
DEFINE_HOOK_SYMBOL("dName_c::nameWide", void(dName_c*), NameWide);
DEFINE_HOOK_SYMBOL("dBrightCheck_c::brightCheckWide", void(dBrightCheck_c*), BrightCheckWide);
DEFINE_HOOK_SYMBOL("dFile_select_c::fileSelectWide", void(dFile_select_c*), FileSelectWide);
DEFINE_HOOK_SYMBOL("dDlst_MenuSave_c::draw", void(dDlst_MenuSave_c*), MenuSaveDraw);
DEFINE_HOOK_SYMBOL("dDlst_FileSel_c::draw", void(dDlst_FileSel_c*), FileSelDraw);
DEFINE_HOOK_SYMBOL("dDlst_FileSelDt_c::draw", void(dDlst_FileSelDt_c*), FileSelDtDraw);
DEFINE_HOOK_SYMBOL("dDlst_FileSelYn_c::draw", void(dDlst_FileSelYn_c*), FileSelYnDraw);
DEFINE_HOOK_SYMBOL("dDlst_FileSel3m_c::draw", void(dDlst_FileSel3m_c*), FileSel3mDraw);

bool s_menu_save_draw_hook_installed = false;
bool s_file_sel_draw_hook_installed = false;
bool s_file_sel_dt_draw_hook_installed = false;
bool s_file_sel_yn_draw_hook_installed = false;
bool s_file_sel_3m_draw_hook_installed = false;
bool s_file_select_details_window_active = false;

const MenuScaling scaling_mode() {
    return runtime_settings().menuScaling;
}

void reset_pane(J2DPane* pane, const PaneCache& cache) {
    if (pane == nullptr) return;
    pane->setBasePosition(J2DBasePosition_4);
    pane->scale(1.0f, 1.0f);
    pane->translate(cache.origTransX, cache.origTransY);
}

void cache_pane(J2DPane* pane, PaneCache& cache) {
    if (pane == nullptr || cache.cached) return;
    cache.origTransX = pane->getTranslateX();
    cache.origTransY = pane->getTranslateY();
    cache.cached = true;
}

void scale_pane(J2DPane* pane, f32 scale) {
    if (pane != nullptr) pane->scale(scale, 1.0f);
}

void translate_pane(J2DPane* pane, f32 x, f32 y) {
    if (pane != nullptr) pane->translate(x, y);
}

static PaneCache s_collect_panes[] = {
    {MULTI_CHAR('sa_tex_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('op_tex_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('heart_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('wolf_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('item_0_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('item_1_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('item_2_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('fish_3_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('lett_4_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('maki_5_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('fuku_n0'), 0.0f, 0.0f, false},
    {MULTI_CHAR('fuku_n1'), 0.0f, 0.0f, false},
    {MULTI_CHAR('fuku_n2'), 0.0f, 0.0f, false},
    {MULTI_CHAR('tate_n0'), 0.0f, 0.0f, false},
    {MULTI_CHAR('tate_n1'), 0.0f, 0.0f, false},
    {MULTI_CHAR('ken_n0'), 0.0f, 0.0f, false},
    {MULTI_CHAR('ken_n1'), 0.0f, 0.0f, false},
    {MULTI_CHAR('kabu_6n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('t_t00'), 0.0f, 0.0f, false},
    {MULTI_CHAR('f_t00'), 0.0f, 0.0f, false},
    {MULTI_CHAR('itemn_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('infotxtn'), 0.0f, 0.0f, false},
    {MULTI_CHAR('sa_op_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('title_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('menu_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('w_er_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('center_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('info_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('lavel_n'), 0.0f, 0.0f, false},
    {MULTI_CHAR('modelbgn'), 0.0f, 0.0f, false},
};

void collect_wide(dMenu_Collect2D_c* self) {
    if (self == nullptr || self->mpScreen == nullptr || self->mpScreenIcon == nullptr) return;

    for (PaneCache& entry : s_collect_panes) {
        cache_pane(self->mpScreen->search(entry.tag), entry);
        reset_pane(self->mpScreen->search(entry.tag), entry);
    }
    self->mpScreen->scale(1.0f, 1.0f);
    self->mpScreen->translate(0.0f, 0.0f);
    self->mpScreenIcon->translate(0.0f, 0.0f);

    const f32 down = mDoGph_gInf_c::hudAspectScaleDown;
    const f32 up = mDoGph_gInf_c::hudAspectScaleUp;
    switch (scaling_mode()) {
    case MenuScaling::GameCube: {
        constexpr f32 nativeAspect = 4.0f / 3.0f;
        constexpr f32 wideBackgroundScale = (3.0f / 2.0f) / nativeAspect;
        const bool nativeLayout = up <= 1.0001f;
        const f32 backgroundScale = nativeLayout ? 1.0f : wideBackgroundScale;
        self->mpScreen->scale(backgroundScale, 1.0f);
        self->mpScreen->translate(FB_WIDTH_BASE * (1.0f - backgroundScale) * 0.5f, 0.0f);
        const f32 foregroundScale = 1.0f / backgroundScale;
        for (int i = 0; i < 22; ++i) {
            scale_pane(self->mpScreen->search(s_collect_panes[i].tag), foregroundScale);
        }
        if (self->mpDrawCursor) self->mpDrawCursor->refreshAspectScale(1.0f);
        break;
    }
    case MenuScaling::Wii:
        self->mpScreen->scale(up, 1.0f);
        self->mpScreen->translate(mDoGph_gInf_c::getSafeMinXF(), 0.0f);
        self->mpScreenIcon->translate(-mDoGph_gInf_c::getSafeMinXF(), 0.0f);
        for (int i = 0; i < 22; ++i) scale_pane(self->mpScreen->search(s_collect_panes[i].tag), down);
        if (self->mpDrawCursor) self->mpDrawCursor->refreshAspectScale(up);
        break;
    case MenuScaling::Dusklight: {
        self->mpScreen->scale(up, 1.0f);
        self->mpScreen->translate(mDoGph_gInf_c::getSafeMinXF(), 0.0f);
        for (int i = 22; i < 27; ++i) scale_pane(self->mpScreen->search(s_collect_panes[i].tag), down);

        const f32 leftShift = 48.0f * (up - 1.0f);
        for (int i : {27, 28}) {
            J2DPane* pane = self->mpScreen->search(s_collect_panes[i].tag);
            if (pane != nullptr) {
                pane->translate(s_collect_panes[i].origTransX - leftShift, pane->getTranslateY());
            }
        }
        J2DPane* model = self->mpScreen->search(s_collect_panes[29].tag);
        if (model != nullptr) {
            model->setBasePosition(J2DBasePosition_0);
            model->scale(down, 1.3f);
            const f32 modelFactor = 1.0f + 0.16f * (down - 1.0f);
            model->translate((s_collect_panes[29].origTransX - 12.0f) * modelFactor,
                             model->getTranslateY());
        }
        if (self->mpDrawCursor) self->mpDrawCursor->refreshAspectScale(1.0f);
        break;
    }
    case MenuScaling::Native:
    default:
        break;
    }
}

void collect_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dMenu_Collect2D_c*>(args, 0);
    if (scaling_mode() == MenuScaling::Native) {
        CollectWide::g_orig(self);
        return;
    }
    collect_wide(self);
}

void save_wide(dMenu_save_c* self) {
    if (self == nullptr || self->mSaveSel.Scr == nullptr) return;
    const bool gameCube = scaling_mode() == MenuScaling::GameCube;
    const f32 nativeScale = mDoGph_gInf_c::hudAspectScaleDown;
    const f32 childScale = gameCube ? 1.0f : nativeScale;
    J2DScreen* screen = self->mSaveSel.Scr;
    screen->scale(mDoGph_gInf_c::hudAspectScaleUp, 1.0f);
    screen->translate(mDoGph_gInf_c::getSafeMinXF(), 0.0f);

    for (u64 tag : {MULTI_CHAR('t_for'), MULTI_CHAR('t_for1'), MULTI_CHAR('w_btn_n'),
                    MULTI_CHAR('w_uzu00'), MULTI_CHAR('w_uzu01'), MULTI_CHAR('w_uzu02'),
                    MULTI_CHAR('w_uzu03'), MULTI_CHAR('w_uzu04'), MULTI_CHAR('w_uzu05'),
                    MULTI_CHAR('w_uzu06'), MULTI_CHAR('w_uzu07'), MULTI_CHAR('w_uzu08'),
                    MULTI_CHAR('w_uzu09')}) {
        scale_pane(screen->search(tag), nativeScale);
    }
    for (u64 tag : {MULTI_CHAR('w_n_bk00'), MULTI_CHAR('w_n_bk01'), MULTI_CHAR('w_n_bk02'),
                    MULTI_CHAR('w_dat_i0'), MULTI_CHAR('w_dat_i1'), MULTI_CHAR('w_dat_i2'),
                    MULTI_CHAR('w_no_t'), MULTI_CHAR('f_no_t'), MULTI_CHAR('w_yes_t'),
                    MULTI_CHAR('f_yes_t')}) {
        scale_pane(screen->search(tag), childScale);
    }

    if (self->mSelIcon != nullptr) {
        self->mSelIcon->refreshAspectScale(gameCube ? 1.0f : mDoGph_gInf_c::hudAspectScaleUp);
        if (gameCube && self->field_0x9c != 0 && self->mYesNoCursor != 0xFF &&
            self->mpNoYes[self->mYesNoCursor] != nullptr && self->mpNoYes[0] != nullptr &&
            self->mpNoYes[1] != nullptr) {
            const Vec noPos = self->mpNoYes[dMenu_save_c::CURSOR_NO]->getGlobalVtxCenter(false, 0);
            const Vec yesPos = self->mpNoYes[dMenu_save_c::CURSOR_YES]->getGlobalVtxCenter(false, 0);
            const Vec selectedPos = self->mpNoYes[self->mYesNoCursor]->getGlobalVtxCenter(false, 0);
            const f32 optionMidX = (noPos.x + yesPos.x) * 0.5f;
            const f32 correctedX = optionMidX + (selectedPos.x - optionMidX) * nativeScale;
            J2DPane* selected = self->mpNoYes[self->mYesNoCursor]->getPanePtr();
            self->mSelIcon->setPos(correctedX, selectedPos.y, selected, true);
            self->mSelIcon->setPos(correctedX, selectedPos.y, nullptr, false);
        }
    }
}

void save_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dMenu_save_c*>(args, 0);
    if (scaling_mode() == MenuScaling::Native) {
        SaveWide::g_orig(self);
        return;
    }
    save_wide(self);
}

void menu_save_draw_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dDlst_MenuSave_c*>(args, 0);
    if (scaling_mode() != MenuScaling::GameCube || self == nullptr || self->Scr == nullptr) {
        MenuSaveDraw::g_orig(self);
        return;
    }

    static constexpr u64 nativeTags[] = {
        MULTI_CHAR('w_sel_00'), MULTI_CHAR('w_sel_01'), MULTI_CHAR('w_sel_02'),
        MULTI_CHAR('w_no_n'), MULTI_CHAR('w_yes_n'),
    };
    f32 scaleX[std::size(nativeTags)];
    f32 scaleY[std::size(nativeTags)];
    f32 transX[std::size(nativeTags)];
    f32 transY[std::size(nativeTags)];
    for (size_t i = 0; i < std::size(nativeTags); ++i) {
        J2DPane* pane = self->Scr->search(nativeTags[i]);
        if (pane == nullptr) continue;
        scaleX[i] = pane->getScaleX();
        scaleY[i] = pane->getScaleY();
        transX[i] = pane->getTranslateX();
        transY[i] = pane->getTranslateY();
        pane->scale(scaleX[i] * mDoGph_gInf_c::hudAspectScaleDown, scaleY[i]);
    }

    const f32 optionMidX = (transX[3] + transX[4]) * 0.5f;
    for (size_t i = 3; i < std::size(nativeTags); ++i) {
        J2DPane* pane = self->Scr->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->translate(optionMidX + (transX[i] - optionMidX) *
                                mDoGph_gInf_c::hudAspectScaleDown,
                            transY[i]);
        }
    }

    static constexpr u64 spiralTags[] = {
        MULTI_CHAR('w_uzu00'), MULTI_CHAR('w_uzu01'), MULTI_CHAR('w_uzu02'),
        MULTI_CHAR('w_uzu03'), MULTI_CHAR('w_uzu04'), MULTI_CHAR('w_uzu05'),
        MULTI_CHAR('w_uzu06'), MULTI_CHAR('w_uzu07'), MULTI_CHAR('w_uzu08'),
        MULTI_CHAR('w_uzu09'),
    };
    f32 spiralX[std::size(spiralTags)];
    f32 spiralY[std::size(spiralTags)];
    f32 spiralMinX = 100000.0f;
    f32 spiralMaxX = -100000.0f;
    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane == nullptr) continue;
        spiralX[i] = pane->getTranslateX();
        spiralY[i] = pane->getTranslateY();
        spiralMinX = std::min(spiralMinX, spiralX[i]);
        spiralMaxX = std::max(spiralMaxX, spiralX[i]);
    }
    const f32 spiralMidX = (spiralMinX + spiralMaxX) * 0.5f;
    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane != nullptr) {
            pane->translate(spiralMidX + (spiralX[i] - spiralMidX) *
                                mDoGph_gInf_c::hudAspectScaleDown,
                            spiralY[i]);
        }
    }

    self->Scr->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());

    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane != nullptr) pane->translate(spiralX[i], spiralY[i]);
    }
    for (size_t i = 0; i < std::size(nativeTags); ++i) {
        J2DPane* pane = self->Scr->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->scale(scaleX[i], scaleY[i]);
            pane->translate(transX[i], transY[i]);
        }
    }
}

static PaneCache s_bright_panes[] = {
    {MULTI_CHAR('fuchi_1'), 0, 0, false}, {MULTI_CHAR('big_squa'), 0, 0, false},
    {MULTI_CHAR('fuchi_3'), 0, 0, false}, {MULTI_CHAR('big_squ1'), 0, 0, false},
    {MULTI_CHAR('fuchi_4'), 0, 0, false}, {MULTI_CHAR('big_squ2'), 0, 0, false},
    {MULTI_CHAR('gray_n'), 0, 0, false}, {MULTI_CHAR('fuchi_2'), 0, 0, false},
    {MULTI_CHAR('abtn_n'), 0, 0, false}, {MULTI_CHAR('gcabtn_n'), 0, 0, false},
    {MULTI_CHAR('menu_6n'), 0, 0, false}, {MULTI_CHAR('menu_9n'), 0, 0, false},
    {MULTI_CHAR('menu_10n'), 0, 0, false}, {MULTI_CHAR('menu_7n'), 0, 0, false},
    {MULTI_CHAR('menu_8n'), 0, 0, false}, {MULTI_CHAR('fmenu_8n'), 0, 0, false},
    {MULTI_CHAR('fmenu_7n'), 0, 0, false}, {MULTI_CHAR('fmenu_10'), 0, 0, false},
    {MULTI_CHAR('fmenu_6n'), 0, 0, false}, {MULTI_CHAR('fmenu_9n'), 0, 0, false},
    {MULTI_CHAR('t_t00'), 0, 0, false}, {MULTI_CHAR('f_t00'), 0, 0, false},
    {MULTI_CHAR('t_mo_l_n'), 0, 0, false}, {MULTI_CHAR('t_mo_r_n'), 0, 0, false},
};

void bright_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dBrightCheck_c*>(args, 0);
    if (scaling_mode() == MenuScaling::Native) {
        BrightCheckWide::g_orig(self);
        return;
    }
    if (self == nullptr || self->mBrightCheck.Scr == nullptr) return;
    J2DScreen* screen = self->mBrightCheck.Scr;
    const f32 up = mDoGph_gInf_c::hudAspectScaleUp;
    const bool gameCube = scaling_mode() == MenuScaling::GameCube;
    constexpr f32 targetScale3x2 = (3.0f / 2.0f) / (4.0f / 3.0f);
    const f32 canvasScale = gameCube && up > targetScale3x2 ? targetScale3x2 : up;
    const f32 down = 1.0f / canvasScale;
    const f32 canvasWidth = FB_WIDTH_BASE * canvasScale;
    const f32 canvasX = gameCube
        ? mDoGph_gInf_c::getSafeMinXF() +
              (mDoGph_gInf_c::getSafeWidthF() - canvasWidth) * 0.5f
        : mDoGph_gInf_c::getSafeMinXF();
    screen->scale(canvasScale, 1.0f);
    screen->translate(canvasX, 0.0f);
    for (PaneCache& entry : s_bright_panes) {
        scale_pane(screen->search(entry.tag), down);
    }
}

static PaneCache s_name_chars[] = {
    {MULTI_CHAR('m_00_0'),0,0,false},{MULTI_CHAR('m_00_1'),0,0,false},{MULTI_CHAR('m_00_2'),0,0,false},{MULTI_CHAR('m_00_3'),0,0,false},{MULTI_CHAR('m_00_4'),0,0,false},
    {MULTI_CHAR('m_01_0'),0,0,false},{MULTI_CHAR('m_01_1'),0,0,false},{MULTI_CHAR('m_01_2'),0,0,false},{MULTI_CHAR('m_01_3'),0,0,false},{MULTI_CHAR('m_01_4'),0,0,false},
    {MULTI_CHAR('m_02_0'),0,0,false},{MULTI_CHAR('m_02_1'),0,0,false},{MULTI_CHAR('m_02_2'),0,0,false},{MULTI_CHAR('m_02_3'),0,0,false},{MULTI_CHAR('m_02_4'),0,0,false},
    {MULTI_CHAR('m03_0'),0,0,false},{MULTI_CHAR('m03_1'),0,0,false},{MULTI_CHAR('m03_2'),0,0,false},{MULTI_CHAR('m03_3'),0,0,false},{MULTI_CHAR('m03_4'),0,0,false},
    {MULTI_CHAR('m_04_0'),0,0,false},{MULTI_CHAR('m_04_1'),0,0,false},{MULTI_CHAR('m_04_2'),0,0,false},{MULTI_CHAR('m_04_3'),0,0,false},{MULTI_CHAR('m_04_4'),0,0,false},
    {MULTI_CHAR('m_05_0'),0,0,false},{MULTI_CHAR('m_05_1'),0,0,false},{MULTI_CHAR('m_05_2'),0,0,false},{MULTI_CHAR('m_05_3'),0,0,false},{MULTI_CHAR('m_05_4'),0,0,false},
    {MULTI_CHAR('m_06_0'),0,0,false},{MULTI_CHAR('m_06_1'),0,0,false},{MULTI_CHAR('m_06_2'),0,0,false},{MULTI_CHAR('m_06_3'),0,0,false},{MULTI_CHAR('m_06_4'),0,0,false},
    {MULTI_CHAR('m_07_0'),0,0,false},{MULTI_CHAR('m_07_1'),0,0,false},{MULTI_CHAR('m_07_2'),0,0,false},{MULTI_CHAR('m_07_3'),0,0,false},{MULTI_CHAR('m_07_4'),0,0,false},
    {MULTI_CHAR('m_08_0'),0,0,false},{MULTI_CHAR('m_08_1'),0,0,false},{MULTI_CHAR('m_08_2'),0,0,false},{MULTI_CHAR('m_08_3'),0,0,false},{MULTI_CHAR('m_08_4'),0,0,false},
    {MULTI_CHAR('m_09_0'),0,0,false},{MULTI_CHAR('m_09_1'),0,0,false},{MULTI_CHAR('m_09_2'),0,0,false},{MULTI_CHAR('m_09_3'),0,0,false},{MULTI_CHAR('m_09_4'),0,0,false},
    {MULTI_CHAR('m_10_0'),0,0,false},{MULTI_CHAR('m_10_1'),0,0,false},{MULTI_CHAR('m_10_2'),0,0,false},{MULTI_CHAR('m_10_3'),0,0,false},{MULTI_CHAR('m_10_4'),0,0,false},
    {MULTI_CHAR('m_11_0'),0,0,false},{MULTI_CHAR('m_11_1'),0,0,false},{MULTI_CHAR('m_11_2'),0,0,false},{MULTI_CHAR('m_11_3'),0,0,false},{MULTI_CHAR('m_11_4'),0,0,false},
    {MULTI_CHAR('m12_0'),0,0,false},{MULTI_CHAR('m12_1'),0,0,false},{MULTI_CHAR('m12_2'),0,0,false},{MULTI_CHAR('m12_3'),0,0,false},{MULTI_CHAR('m12_4'),0,0,false},
};
static PaneCache s_name_text[] = {
    {MULTI_CHAR('name_00'),0,0,false},{MULTI_CHAR('name_01'),0,0,false},{MULTI_CHAR('name_02'),0,0,false},{MULTI_CHAR('name_03'),0,0,false},
    {MULTI_CHAR('name_04'),0,0,false},{MULTI_CHAR('name_05'),0,0,false},{MULTI_CHAR('name_06'),0,0,false},{MULTI_CHAR('name_07'),0,0,false},
};
static PaneCache s_name_cursor[] = {
    {MULTI_CHAR('s__n_00'),0,0,false},{MULTI_CHAR('s__n_01'),0,0,false},{MULTI_CHAR('s__n_02'),0,0,false},{MULTI_CHAR('s__n_03'),0,0,false},
    {MULTI_CHAR('s__n_04'),0,0,false},{MULTI_CHAR('s__n_05'),0,0,false},{MULTI_CHAR('s__n_06'),0,0,false},{MULTI_CHAR('s__n_07'),0,0,false},
};

void name_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dName_c*>(args, 0);
    if (scaling_mode() == MenuScaling::Native) {
        NameWide::g_orig(self);
        return;
    }
    if (self == nullptr || self->nameIn.NameInScr == nullptr) return;
    J2DScreen* screen = self->nameIn.NameInScr;
    auto reset_group = [&](PaneCache* group, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            PaneCache& entry = group[i];
            J2DPane* pane = screen->search(entry.tag);
            cache_pane(pane, entry);
            reset_pane(pane, entry);
        }
    };
    reset_group(s_name_chars, std::size(s_name_chars));
    reset_group(s_name_text, std::size(s_name_text));
    reset_group(s_name_cursor, std::size(s_name_cursor));
    if (scaling_mode() != MenuScaling::GameCube) {
        for (PaneCache& entry : s_name_chars) scale_pane(screen->search(entry.tag), mDoGph_gInf_c::hudAspectScaleDown);
        for (PaneCache& entry : s_name_text) scale_pane(screen->search(entry.tag), mDoGph_gInf_c::hudAspectScaleDown);
        for (PaneCache& entry : s_name_cursor) scale_pane(screen->search(entry.tag), mDoGph_gInf_c::hudAspectScaleDown);
        if (self->mSelIcon) self->mSelIcon->refreshAspectScale(mDoGph_gInf_c::hudAspectScaleUp);
    } else if (self->mSelIcon) {
        self->mSelIcon->refreshAspectScale(1.0f);
    }
}

static PaneCache s_file_details[] = {
    {MULTI_CHAR('tate_n0'), 0, 0, false}, {MULTI_CHAR('tate_n1'), 0, 0, false},
    {MULTI_CHAR('ken_n0'), 0, 0, false}, {MULTI_CHAR('ken_n1'), 0, 0, false},
    {MULTI_CHAR('fuku_n0'), 0, 0, false}, {MULTI_CHAR('fuku_n1'), 0, 0, false},
    {MULTI_CHAR('fuku_n2'), 0, 0, false}, {MULTI_CHAR('gray_n'), 0, 0, false},
    {MULTI_CHAR('b_base'), 0, 0, false}, {MULTI_CHAR('b_base1'), 0, 0, false},
};
static PaneCache s_file_panes[] = {
    {MULTI_CHAR('w_uzu00'), 0, 0, false}, {MULTI_CHAR('w_uzu01'), 0, 0, false},
    {MULTI_CHAR('w_uzu02'), 0, 0, false}, {MULTI_CHAR('w_uzu03'), 0, 0, false},
    {MULTI_CHAR('w_uzu04'), 0, 0, false}, {MULTI_CHAR('w_uzu05'), 0, 0, false},
    {MULTI_CHAR('w_uzu06'), 0, 0, false}, {MULTI_CHAR('w_uzu07'), 0, 0, false},
    {MULTI_CHAR('w_uzu08'), 0, 0, false}, {MULTI_CHAR('w_uzu09'), 0, 0, false},
    {MULTI_CHAR('w_er_msg'), 0, 0, false}, {MULTI_CHAR('w_er_msE'), 0, 0, false},
    {MULTI_CHAR('w_er_msR'), 0, 0, false}, {MULTI_CHAR('er_for0'), 0, 0, false},
    {MULTI_CHAR('er_for1'), 0, 0, false},
};

void file_select_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dFile_select_c*>(args, 0);
    if (scaling_mode() == MenuScaling::Native) {
        s_file_select_details_window_active = false;
        FileSelectWide::g_orig(self);
        return;
    }
    if (self == nullptr || self->mSelDt.ScrDt == nullptr || self->fileSel.Scr == nullptr ||
        self->mYnSel.ScrYn == nullptr || self->m3mSel.Scr3m == nullptr) return;

    for (PaneCache& entry : s_file_details) {
        J2DPane* pane = self->mSelDt.ScrDt->search(entry.tag);
        cache_pane(pane, entry);
        reset_pane(pane, entry);
    }
    for (PaneCache& entry : s_file_panes) {
        J2DPane* pane = self->fileSel.Scr->search(entry.tag);
        cache_pane(pane, entry);
        reset_pane(pane, entry);
    }

    const MenuScaling mode = scaling_mode();
    const bool gameCube = mode == MenuScaling::GameCube;
    s_file_select_details_window_active =
        gameCube && self->mDataSelProc >= dFile_select_c::DATASELPROC_SELECT_DATA_OPEN_MOVE;
    const f32 root = mDoGph_gInf_c::hudAspectScaleUp;
    const f32 child = mDoGph_gInf_c::hudAspectScaleDown;
    const f32 animated = gameCube ? 1.0f : child;
    const f32 rootX = mDoGph_gInf_c::getSafeMinXF();
    self->mYnSel.ScrYn->scale(root, 1.0f);
    self->mYnSel.ScrYn->translate(rootX, 0.0f);
    for (u64 tag : {MULTI_CHAR('w_no_t'), MULTI_CHAR('f_no_t'), MULTI_CHAR('w_yes_t'), MULTI_CHAR('f_yes_t')})
        scale_pane(self->mYnSel.ScrYn->search(tag), animated);
    self->m3mSel.Scr3m->scale(root, 1.0f);
    self->m3mSel.Scr3m->translate(rootX, 0.0f);
    for (u64 tag : {MULTI_CHAR('w_sta'), MULTI_CHAR('f_sta'), MULTI_CHAR('w_del'), MULTI_CHAR('f_del'), MULTI_CHAR('w_cop_t'), MULTI_CHAR('f_cop_t')})
        scale_pane(self->m3mSel.Scr3m->search(tag), animated);
    self->fileSel.Scr->scale(root, 1.0f);
    self->fileSel.Scr->translate(rootX, 0.0f);
    for (u64 tag : {MULTI_CHAR('t_for'), MULTI_CHAR('t_for1'), MULTI_CHAR('w_btn_n')})
        scale_pane(self->fileSel.Scr->search(tag), child);
    for (u64 tag : {MULTI_CHAR('w_n_bk00'), MULTI_CHAR('w_n_bk01'), MULTI_CHAR('w_n_bk02'), MULTI_CHAR('w_dat_i0'), MULTI_CHAR('w_dat_i1'), MULTI_CHAR('w_dat_i2')})
        scale_pane(self->fileSel.Scr->search(tag), animated);
    if (self->mCpSel.Scr != nullptr) {
        for (u64 tag : {MULTI_CHAR('w_dat_i1'), MULTI_CHAR('w_dat_i2'), MULTI_CHAR('w_n_bk01'), MULTI_CHAR('w_n_bk02')})
            scale_pane(self->mCpSel.Scr->search(tag), child);
    }

    switch (mode) {
    case MenuScaling::GameCube:
        for (PaneCache& entry : s_file_panes) scale_pane(self->fileSel.Scr->search(entry.tag), child);
        if (self->mSelIcon != nullptr) self->mSelIcon->refreshAspectScale(1.0f);
        if (self->mSelIcon2 != nullptr) self->mSelIcon2->refreshAspectScale(1.0f);
        break;
    case MenuScaling::Wii:
        for (PaneCache& entry : s_file_details) scale_pane(self->mSelDt.ScrDt->search(entry.tag), child);
        for (PaneCache& entry : s_file_panes) scale_pane(self->fileSel.Scr->search(entry.tag), child);
        if (self->mSelIcon != nullptr) self->mSelIcon->refreshAspectScale(root);
        if (self->mSelIcon2 != nullptr) self->mSelIcon2->refreshAspectScale(root);
        break;
    case MenuScaling::Dusklight: {
        constexpr f32 minAspect = 4.0f / 2.94f;
        constexpr f32 wideAspect = 16.0f / 9.0f + 0.05f;
        constexpr f32 ultraAspect = 21.0f / 9.0f + 0.05f;
        const f32 aspect = mDoGph_gInf_c::getAspect();
        const f32 wideScale = 1.0f + 0.16f * (root - 1.0f);
        const f32 ultraScale = 1.0f + 0.115f * (root - 1.0f);
        J2DPane* gray = self->mSelDt.ScrDt->search(MULTI_CHAR('gray_n'));
        const f32 grayX = gray != nullptr ? gray->getTranslateX() : 0.0f;
        const f32 wideShift = grayX * (wideScale - child);
        const f32 ultraShift = grayX * (ultraScale - child);
        for (size_t i = 0; i < std::size(s_file_details); ++i) {
            PaneCache& entry = s_file_details[i];
            J2DPane* pane = self->mSelDt.ScrDt->search(entry.tag);
            if (pane == nullptr) continue;
            pane->setBasePosition(J2DBasePosition_0);
            pane->scale(child, 1.0f);
            if (aspect >= minAspect && aspect <= wideAspect) {
                if (entry.tag == MULTI_CHAR('b_base')) pane->translate((entry.origTransX + (aspect > 1.75f ? 11.0f : 8.0f)) * wideScale, pane->getTranslateY());
                if (entry.tag == MULTI_CHAR('b_base1')) pane->translate((entry.origTransX + (aspect > 1.75f ? -21.5f : -12.0f)) * wideScale, pane->getTranslateY());
                if (entry.tag == MULTI_CHAR('gray_n')) pane->translate(entry.origTransX * wideScale, pane->getTranslateY());
                if (i <= 6) pane->translate(child * entry.origTransX + wideShift - 60.0f * (1.0f - child), pane->getTranslateY());
            } else if (aspect >= minAspect && aspect <= ultraAspect) {
                if (entry.tag == MULTI_CHAR('b_base')) pane->translate((entry.origTransX + 18.0f) * ultraScale, pane->getTranslateY());
                if (entry.tag == MULTI_CHAR('b_base1')) pane->translate((entry.origTransX - 40.0f) * ultraScale, pane->getTranslateY());
                if (entry.tag == MULTI_CHAR('gray_n')) pane->translate(entry.origTransX * ultraScale, pane->getTranslateY());
                if (i <= 6) pane->translate(child * entry.origTransX + ultraShift - 62.0f * (1.0f - child), pane->getTranslateY());
            } else {
                pane->setBasePosition(J2DBasePosition_4);
                pane->translate(entry.origTransX, pane->getTranslateY());
                if (entry.tag == MULTI_CHAR('gray_n') || entry.tag == MULTI_CHAR('b_base') || entry.tag == MULTI_CHAR('b_base1')) pane->scale(1.0f, 1.0f);
            }
        }
        if (self->mSelIcon != nullptr) self->mSelIcon->refreshAspectScale(root);
        if (self->mSelIcon2 != nullptr) self->mSelIcon2->refreshAspectScale(root);
        for (PaneCache& entry : s_file_panes) scale_pane(self->fileSel.Scr->search(entry.tag), child);
        break;
    }
    case MenuScaling::Native:
    default:
        break;
    }
}

void file_sel_draw_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dDlst_FileSel_c*>(args, 0);
    if (scaling_mode() != MenuScaling::GameCube || self == nullptr || self->Scr == nullptr) {
        FileSelDraw::g_orig(self);
        return;
    }

    static constexpr u64 nativeTags[] = {
        MULTI_CHAR('w_sel_00'), MULTI_CHAR('w_sel_01'), MULTI_CHAR('w_sel_02'),
    };
    f32 scaleX[std::size(nativeTags)];
    f32 scaleY[std::size(nativeTags)];
    for (size_t i = 0; i < std::size(nativeTags); ++i) {
        J2DPane* pane = self->Scr->search(nativeTags[i]);
        if (pane == nullptr) continue;
        scaleX[i] = pane->getScaleX();
        scaleY[i] = pane->getScaleY();
        pane->scale(scaleX[i] * mDoGph_gInf_c::hudAspectScaleDown, scaleY[i]);
    }

    J2DPane* detailsBorder = self->Scr->search(MULTI_CHAR('w_sub_n'));
    f32 detailsScaleX = 1.0f;
    f32 detailsScaleY = 1.0f;
    if (detailsBorder != nullptr) {
        detailsScaleX = detailsBorder->getScaleX();
        detailsScaleY = detailsBorder->getScaleY();
        if (s_file_select_details_window_active) {
            detailsBorder->scale(detailsScaleX * mDoGph_gInf_c::hudAspectScaleDown,
                                 detailsScaleY);
        }
    }

    static constexpr u64 spiralTags[] = {
        MULTI_CHAR('w_uzu00'), MULTI_CHAR('w_uzu01'), MULTI_CHAR('w_uzu02'),
        MULTI_CHAR('w_uzu03'), MULTI_CHAR('w_uzu04'), MULTI_CHAR('w_uzu05'),
        MULTI_CHAR('w_uzu06'), MULTI_CHAR('w_uzu07'), MULTI_CHAR('w_uzu08'),
        MULTI_CHAR('w_uzu09'),
    };
    f32 spiralX[std::size(spiralTags)];
    f32 spiralY[std::size(spiralTags)];
    f32 minX = 100000.0f;
    f32 maxX = -100000.0f;
    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane == nullptr) continue;
        spiralX[i] = pane->getTranslateX();
        spiralY[i] = pane->getTranslateY();
        minX = std::min(minX, spiralX[i]);
        maxX = std::max(maxX, spiralX[i]);
    }
    const f32 midX = (minX + maxX) * 0.5f;
    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane != nullptr) {
            pane->translate(midX + (spiralX[i] - midX) *
                                mDoGph_gInf_c::hudAspectScaleDown,
                            spiralY[i]);
        }
    }

    self->Scr->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());

    for (size_t i = 0; i < std::size(spiralTags); ++i) {
        J2DPane* pane = self->Scr->search(spiralTags[i]);
        if (pane != nullptr) pane->translate(spiralX[i], spiralY[i]);
    }
    for (size_t i = 0; i < std::size(nativeTags); ++i) {
        J2DPane* pane = self->Scr->search(nativeTags[i]);
        if (pane != nullptr) pane->scale(scaleX[i], scaleY[i]);
    }
    if (detailsBorder != nullptr && s_file_select_details_window_active) {
        detailsBorder->scale(detailsScaleX, detailsScaleY);
    }
}

void file_sel_dt_draw_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dDlst_FileSelDt_c*>(args, 0);
    if (scaling_mode() != MenuScaling::GameCube || self == nullptr || self->ScrDt == nullptr ||
        self->mpPane == nullptr || self->mpPane2 == nullptr) {
        FileSelDtDraw::g_orig(self);
        return;
    }

    MtxP local = (MtxP)&self->mpPane->getGlbMtx()[0][0];
    Mtx translated;
    Mtx scaled;
    MTXTrans(translated, self->mpPane->getWidth() * 0.5f,
             self->mpPane->getHeight() * 0.5f, 0.0f);
    MTXConcat(local, translated, local);
    const f32 aspectCorrection = s_file_select_details_window_active
        ? 1.0f : mDoGph_gInf_c::hudAspectScaleDown;
    MTXScale(scaled, (self->mpPane->getWidth() / self->mpPane2->getWidth()) *
                          aspectCorrection,
             self->mpPane->getHeight() / self->mpPane2->getHeight(), 1.0f);
    MTXConcat(local, scaled, local);
    self->mpPane2->setMtx(local);
    self->ScrDt->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());
}

void file_sel_yn_draw_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dDlst_FileSelYn_c*>(args, 0);
    if (scaling_mode() != MenuScaling::GameCube || self == nullptr || self->ScrYn == nullptr) {
        FileSelYnDraw::g_orig(self);
        return;
    }

    static constexpr u64 nativeTags[] = {MULTI_CHAR('w_no_n'), MULTI_CHAR('w_yes_n')};
    f32 scaleX[2];
    f32 scaleY[2];
    f32 transX[2];
    f32 transY[2];
    for (int i = 0; i < 2; ++i) {
        J2DPane* pane = self->ScrYn->search(nativeTags[i]);
        if (pane == nullptr) continue;
        scaleX[i] = pane->getScaleX();
        scaleY[i] = pane->getScaleY();
        transX[i] = pane->getTranslateX();
        transY[i] = pane->getTranslateY();
        pane->scale(scaleX[i] * mDoGph_gInf_c::hudAspectScaleDown, scaleY[i]);
    }
    const f32 midX = (transX[0] + transX[1]) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        J2DPane* pane = self->ScrYn->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->translate(midX + (transX[i] - midX) *
                                mDoGph_gInf_c::hudAspectScaleDown,
                            transY[i]);
        }
    }
    self->ScrYn->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());
    for (int i = 0; i < 2; ++i) {
        J2DPane* pane = self->ScrYn->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->scale(scaleX[i], scaleY[i]);
            pane->translate(transX[i], transY[i]);
        }
    }
}

void file_sel_3m_draw_replace(ModContext*, void* args, void*, void*) {
    auto* self = mods::arg<dDlst_FileSel3m_c*>(args, 0);
    if (scaling_mode() != MenuScaling::GameCube || self == nullptr || self->Scr3m == nullptr) {
        FileSel3mDraw::g_orig(self);
        return;
    }

    static constexpr u64 nativeTags[] = {
        MULTI_CHAR('w_sta_n'), MULTI_CHAR('w_del_n'), MULTI_CHAR('w_cop_n'),
    };
    f32 scaleX[3];
    f32 scaleY[3];
    f32 transX[3];
    f32 transY[3];
    f32 minX = 100000.0f;
    f32 maxX = -100000.0f;
    for (int i = 0; i < 3; ++i) {
        J2DPane* pane = self->Scr3m->search(nativeTags[i]);
        if (pane == nullptr) continue;
        scaleX[i] = pane->getScaleX();
        scaleY[i] = pane->getScaleY();
        transX[i] = pane->getTranslateX();
        transY[i] = pane->getTranslateY();
        minX = std::min(minX, transX[i]);
        maxX = std::max(maxX, transX[i]);
        pane->scale(scaleX[i] * mDoGph_gInf_c::hudAspectScaleDown, scaleY[i]);
    }
    const f32 midX = (minX + maxX) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        J2DPane* pane = self->Scr3m->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->translate(midX + (transX[i] - midX) *
                                mDoGph_gInf_c::hudAspectScaleDown,
                            transY[i]);
        }
    }
    self->Scr3m->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());
    for (int i = 0; i < 3; ++i) {
        J2DPane* pane = self->Scr3m->search(nativeTags[i]);
        if (pane != nullptr) {
            pane->scale(scaleX[i], scaleY[i]);
            pane->translate(transX[i], transY[i]);
        }
    }
}

}  // namespace

ModResult install_hooks() {
    ModResult result = mods::hook::replace<CollectWide>(collect_replace);
    if (result != MOD_OK) return result;
    result = mods::hook::replace<SaveWide>(save_replace);
    if (result != MOD_OK) return result;
    result = mods::hook::replace<NameWide>(name_replace);
    if (result != MOD_OK) return result;
    result = mods::hook::replace<BrightCheckWide>(bright_replace);
    if (result != MOD_OK) return result;
    result = mods::hook::replace<FileSelectWide>(file_select_replace);
    if (result != MOD_OK) return result;
    result = mods::hook::replace<MenuSaveDraw>(menu_save_draw_replace);
    if (result != MOD_OK) return result;
    s_menu_save_draw_hook_installed = true;
    result = mods::hook::replace<FileSelDraw>(file_sel_draw_replace);
    if (result != MOD_OK) return result;
    s_file_sel_draw_hook_installed = true;
    result = mods::hook::replace<FileSelDtDraw>(file_sel_dt_draw_replace);
    if (result != MOD_OK) return result;
    s_file_sel_dt_draw_hook_installed = true;
    result = mods::hook::replace<FileSelYnDraw>(file_sel_yn_draw_replace);
    if (result != MOD_OK) return result;
    s_file_sel_yn_draw_hook_installed = true;
    result = mods::hook::replace<FileSel3mDraw>(file_sel_3m_draw_replace);
    if (result != MOD_OK) return result;
    s_file_sel_3m_draw_hook_installed = true;
    return MOD_OK;
}

void uninstall_hooks() {
    if (s_file_sel_3m_draw_hook_installed) {
        mods::hook::uninstall<FileSel3mDraw>();
        s_file_sel_3m_draw_hook_installed = false;
    }
    if (s_file_sel_yn_draw_hook_installed) {
        mods::hook::uninstall<FileSelYnDraw>();
        s_file_sel_yn_draw_hook_installed = false;
    }
    if (s_file_sel_dt_draw_hook_installed) {
        mods::hook::uninstall<FileSelDtDraw>();
        s_file_sel_dt_draw_hook_installed = false;
    }
    if (s_file_sel_draw_hook_installed) {
        mods::hook::uninstall<FileSelDraw>();
        s_file_sel_draw_hook_installed = false;
    }
    if (s_menu_save_draw_hook_installed) {
        mods::hook::uninstall<MenuSaveDraw>();
        s_menu_save_draw_hook_installed = false;
    }
    mods::hook::uninstall<FileSelectWide>();
    mods::hook::uninstall<BrightCheckWide>();
    mods::hook::uninstall<NameWide>();
    mods::hook::uninstall<SaveWide>();
    mods::hook::uninstall<CollectWide>();
    s_file_select_details_window_active = false;
}

}  // namespace twilight_visuals::menu_scaling
