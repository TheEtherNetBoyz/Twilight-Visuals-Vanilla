#include "native_particles.hpp"
#include "geometry.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_kankyo_rain.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_camera_mng.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "SSystem/SComponent/c_counter.h"
#include "SSystem/SComponent/c_math.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"
#include "interpolation.hpp"
#include <cstring>
namespace twilight_visuals::native_particles {
static void dKyr_set_btitex_common(TGXTexObj* i_obj, ResTIMG* i_img, GXTexMapID i_mapID) {
#ifdef TARGET_PC
    i_obj->reset();
#endif
    GXInitTexObj(i_obj, (&i_img->format + i_img->imageOffset), i_img->width, i_img->height,
                 (GXTexFmt)i_img->format, (GXTexWrapMode)i_img->wrapS, (GXTexWrapMode)i_img->wrapT,
                 (GXBool)(i_img->mipmapCount > 1));

    GXInitTexObjLOD(i_obj, (GXTexFilter)i_img->minFilter, (GXTexFilter)i_img->magFilter,
                    i_img->minLOD * 0.125f, i_img->maxLOD * 0.125f, i_img->LODBias * 0.01f,
                    (GXBool)i_img->biasClamp, (GXBool)i_img->doEdgeLOD,
                    (GXAnisotropy)i_img->maxAnisotropy);

    GXLoadTexObj(i_obj, i_mapID);
}

static void dKyr_set_btitex(TGXTexObj* i_obj, ResTIMG* i_img) {
    dKyr_set_btitex_common(i_obj, i_img, GX_TEXMAP0);
}

#if TARGET_PC
template <int N>
struct CachedTexObjs {
    TGXTexObj texObj[N];
    ResTIMG* timg[N] = {};
};

template <int N>
static GXTexObj* load_cached_tex(CachedTexObjs<N>& cache, ResTIMG* img, GXTexMapID mapID) {
    for (int i = 0; i < N; i++) {
        if (img != nullptr && cache.timg[i] == img) {
            GXLoadTexObj(&cache.texObj[i], mapID);
            return &cache.texObj[i];
        }
    }

    int slot = 0;
    for (int i = 0; i < N; i++) {
        if (cache.timg[i] == nullptr) {
            slot = i;
            break;
        }
    }

    if (cache.timg[slot] != nullptr) {
        cache.texObj[slot].reset();
    }
    cache.timg[slot] = img;
    dKyr_set_btitex_common(&cache.texObj[slot], img, mapID);
    return &cache.texObj[slot];
}
#endif

static void dKr_cullVtx_Set(IF_DUSK(bool const vtxColor = false)) {
    GXSetCullMode(GX_CULL_NONE);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_RGBA4, 8);
#if TARGET_PC
    if (vtxColor) {
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    }
#endif
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
#if TARGET_PC
    if (vtxColor) {
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    }
#endif
}
static BOOL d_krain_cut_turn_check() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    BOOL ret = FALSE;

    if (player != NULL && (player->getCutType() == daPy_py_c::CUT_TYPE_TURN_RIGHT ||
                              player->getCutType() == daPy_py_c::CUT_TYPE_TURN_LEFT ||
                              player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT ||
                              player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT)) {
        ret = TRUE;
    }
    return ret;
}

void move(f32 particleTimeScale) {
    dKankyo_housi_Packet* housi_packet = g_env_light.mpHousiPacket;
    HOUSI_EFF* effect;
    camera_class* camera = (camera_class*)dComIfGp_getCamera(0);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    cXyz sp84;
    cXyz sp78 = dKyw_get_wind_vecpow();
    cXyz sp6C;
    cXyz sp60;

    dBgS_GndChk gndchk;
    bool var_r27 = 0;
    f32 var_f31 = 1.0f;

    dBgS_CamGndChk_Wtr cam_gndchk;
    f32 var_f30 = -100000000.0f;
    bool var_r24 = 0;

    cXyz sp54;

    if (strcmp(dComIfGp_getStartStageName(), "D_MN08") == 0) {
        var_r24 = 1;
    }

    if (g_env_light.field_0xea9 == 2) {
        sp54 = camera->view.lookat.eye;
        sp54.y += 100000.0f;

        cam_gndchk.SetPos(&sp54);
        var_f30 = dComIfG_Bgsp().GroundCross(&cam_gndchk);
    }

    if (dKy_darkworld_check() == true || var_r24 == 1 || true) {
        sp78.x = 0.0f;
        sp78.y = 2.8f;
        sp78.z = 0.0f;
    }

    if (g_env_light.field_0xea9 == 1) {
        sp78.x = 0.0f;
        sp78.y = -0.55f;
        sp78.z = 0.0f;
    }

    if (g_env_light.mHousiCount != 0 ||
        (g_env_light.mHousiCount == 0 && housi_packet->field_0x5de8 <= 0.0f))
    {
        housi_packet->mHousiCount = g_env_light.mHousiCount;
    }

    if (g_env_light.mHousiCount != 0) {
        cLib_addCalc(&housi_packet->field_0x5de8, 1.0f, 0.2f, 0.05f, 0.01f);
    } else {
        cLib_addCalc(&housi_packet->field_0x5de8, 0.0f, 0.2f, 0.05f, 0.01f);
    }

    if (housi_packet->mHousiCount == 0) {
        return;
    }

    dKy_set_eyevect_calc2(camera, &sp84, 800.0f, 800.0f);

    if (sp84.abs(housi_packet->field_0x10) > 500.0f) {
        var_r27 = 1;
    }

    housi_packet->field_0x10 = sp84;
    dKyw_get_wind_pow();

    if (g_env_light.field_0xea9 == 1) {
        var_f31 = 0.0f;

        if (g_env_light.camera_water_in_status) {
            dBgS_CamGndChk_Wtr sp90;

            cXyz sp48;
            camera_process_class* cam_p = dComIfGp_getCamera(0);
            sp48 = cam_p->view.lookat.eye;
            sp48.y += 100000.0f;

            sp90.SetPos(&sp48);
            f32 gnd_cross = dComIfG_Bgsp().GroundCross(&sp90);
            if (gnd_cross > cam_p->view.lookat.eye.y) {
                var_f31 = (gnd_cross - cam_p->view.lookat.eye.y) / 700.0f;
                if (var_f31 < 0.0f) {
                    var_f31 = 0.0f;
                }

                if (var_f31 > 1.0f) {
                    var_f31 = 1.0f;
                }
            }
        }
    }

    for (int i = housi_packet->mHousiCount - 1; i >= 0; i--) {
        f32 var_f26 = 0.4f * housi_packet->field_0x5de8;
        effect = &housi_packet->mHousiEff[i];
#if TARGET_PC
        bool skipInterpolation = effect->mStatus == 0;
#endif

        switch (housi_packet->mHousiEff[i].mStatus) {
        case 0:
            if (g_env_light.field_0xea9 == 1) {
                effect->field_0x34 = cM_rndF(0.5f) + 0.1f;
            } else {
                effect->field_0x34 = cM_rndF(1.5f) + 0.2f;
            }

            effect->field_0x3c = 0;
            effect->field_0x4c = cM_rndFX(65536.0f);
            effect->mBasePos.x = sp84.x;
            effect->mBasePos.y = sp84.y;
            effect->mBasePos.z = sp84.z;
            effect->mPosition.x = cM_rndFX(1000.0f);
            effect->mPosition.y = cM_rndFX(1000.0f);
            effect->mPosition.z = cM_rndFX(1000.0f);
            effect->mAlpha = 0.0f;
            effect->field_0x48 = 0.0f;
            effect->mScale.x = cM_rndF(360.0f);
            effect->mScale.y = cM_rndF(360.0f);
            effect->mScale.z = cM_rndF(360.0f);
            effect->mSpeed.x = 0.0f;
            effect->mSpeed.y = 0.0f;
            effect->mSpeed.z = 0.0f;

            if (effect->mBasePos.y + effect->mPosition.y < -100149.9f) {
                effect->mPosition.y = (-99999.9f - effect->mBasePos.y) + 10.0f;
            }

            effect->field_0x38 = 0.0f;
            effect->field_0x44 = 0.0f;
            effect->mStatus++;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            f32 var_f24 = 2.5f;

            if (effect->mStatus != 4) {
                f32 var_f23 = effect->field_0x34 * particleTimeScale;
                if (effect->mStatus == 2) {
                    var_f23 *= 0.25f;
                }

                f32 temp_f0_5 = cM_fsin(effect->mScale.x);
                if (g_env_light.field_0xea9 == 2) {
                    var_f24 = 5.0f;
                }

                if (effect->mStatus != 3) {
                    effect->mPosition.y += var_f23 * (sp78.y * var_f24);
                    effect->mPosition.x += var_f23 * (sp78.x * var_f24);
                    effect->mPosition.y -= var_f23 * 0.6f;

                    if (g_env_light.field_0xea9 == 2) {
                        if (g_env_light.fishing_hole_season == 3) {
                            effect->mPosition.y -= var_f23 * 3.0f;
                        } else {
                            effect->mPosition.y -= var_f23 * 1.5f;
                        }
                    }

                    effect->mPosition.z += var_f23 * (sp78.z * var_f24);
                } else {
                    var_f23 *= 4.5f;

                    effect->mPosition.x += var_f23 * (sp78.x * var_f24);
                    effect->mPosition.y += (var_f23 * (sp78.y * var_f24)) * 0.75f;
                    effect->mPosition.y += var_f23 * 0.3f;
                    effect->mPosition.z += var_f23 * (sp78.z * var_f24);
                }

                effect->mPosition.x += temp_f0_5 * var_f23;
                effect->mPosition.y += var_f23 * 0.5f * cM_fsin(effect->mScale.y);
                effect->mPosition.z += cM_fsin(effect->mScale.z) * var_f23;
            } else if (d_krain_cut_turn_check()) {
                effect->mStatus = 3;
            }

            effect->mScale.x += 0.03f * particleTimeScale;
            effect->mScale.y += 0.02f * particleTimeScale;
            effect->mScale.z += 0.01f * particleTimeScale;

            sp6C.x = effect->mBasePos.x + effect->mPosition.x;
            sp6C.y = effect->mBasePos.y + effect->mPosition.y;
            sp6C.z = effect->mBasePos.z + effect->mPosition.z;

            if (g_env_light.field_0xea9 == 2) {
                cXyz sp3C(sp6C);

                if (sp6C.y <= var_f30) {
                    effect->mStatus = 2;
                }

                if (effect->mStatus == 2) {
                    effect->mPosition.y = var_f30 - effect->mBasePos.y;
                } else if (effect->mStatus != 3 && effect->mStatus != 4) {
                    sp3C.y = player->current.pos.y;

                    if (sp3C.abs(player->current.pos) < 300.0f) {
                        if (sp3C.z > 5600.0f && player->current.pos.y < 130.0f) {
                            if (sp6C.y < player->current.pos.y + 2.0f) {
                                effect->mPosition.y =
                                    (player->current.pos.y + 2.0f) - effect->mBasePos.y;
                                effect->mStatus = 4;
                            }
                        } else {
                            effect->mStatus = 3;
                        }
                    }
                } else {
                    if (effect->mStatus == 4) {
                        effect->mPosition.y =
                            (player->current.pos.y + 2.0f) - effect->mBasePos.y;
                    }

                    if (sp3C.abs(player->current.pos) > 400.0f) {
                        effect->mStatus = 1;
                    }
                }
            }

            sp60 = dKyw_pntwind_get_vecpow(&sp6C);

            if (effect->mSpeed.x < 30.0f) {
                effect->mSpeed.x += sp60.x * 9.0f;
            }

            if (effect->mSpeed.y < 30.0f) {
                effect->mSpeed.y += sp60.y * 9.0f;
            }

            if (effect->mSpeed.z < 30.0f) {
                effect->mSpeed.z += sp60.z * 9.0f;
            }

            cLib_addCalc(&effect->mSpeed.x, 0.0f, 0.2f, 0.1f, 0.00001f);
            cLib_addCalc(&effect->mSpeed.y, 0.0f, 0.2f, 0.1f, 0.00001f);
            cLib_addCalc(&effect->mSpeed.z, 0.0f, 0.2f, 0.1f, 0.00001f);

            effect->mPosition.x += effect->mSpeed.x * particleTimeScale;
            effect->mPosition.y += effect->mSpeed.y * particleTimeScale;
            effect->mPosition.z += effect->mSpeed.z * particleTimeScale;

            sp6C.x = effect->mBasePos.x + effect->mPosition.x;
            sp6C.y = effect->mBasePos.y + effect->mPosition.y;
            sp6C.z = effect->mBasePos.z + effect->mPosition.z;

            f32 var_f1_4 = sp6C.abs(sp84);

            if (effect->field_0x3c == 0) {
                if (var_f1_4 > 1000.0f || sp6C.y < -99979.9f) {
#if TARGET_PC
                    skipInterpolation = true;
#endif
                    effect->field_0x3c = 10;
                    effect->mBasePos = sp84;

                    if (sp6C.abs(sp84) > 1050.0f) {
                        effect->mPosition.x = cM_rndFX(1000.0f);
                        effect->mPosition.y = cM_rndFX(1000.0f);
                        effect->mPosition.z = cM_rndFX(1000.0f);
                    } else {
                        f32 temp_f23 = cM_rndFX(50.0f);
                        dKyr_get_vectle_calc(&sp6C, &sp84, &sp60);

                        effect->mPosition.x = sp60.x * (temp_f23 + 1000.0f);
                        effect->mPosition.y = sp60.y * (temp_f23 + 1000.0f);
                        effect->mPosition.z = sp60.z * (temp_f23 + 1000.0f);
                    }

                    sp6C.x = effect->mBasePos.x + effect->mPosition.x;
                    sp6C.y = effect->mBasePos.y + effect->mPosition.y;
                    sp6C.z = effect->mBasePos.z + effect->mPosition.z;

                    if (sp6C.y <= var_f30) {
                        effect->mPosition.y += 1000.0f;
                    }

                    effect->mSpeed.x = 0.0f;
                    effect->mSpeed.y = 0.0f;
                    effect->mSpeed.z = 0.0f;

                    if (g_env_light.field_0xea9 == 2) {
                        effect->mPosition.y += cM_rndF(3200.0f);
                        if (sp6C.y > 3200.0f) {
                            effect->mPosition.y = 3200.0f - effect->mBasePos.y;
                        }

                        if (g_env_light.fishing_hole_season == 1) {
                            if (sp6C.x > 600.0f || sp6C.z > 1600.0f) {
                                effect->mStatus = 1;
                            } else {
                                effect->mStatus = 2;
                            }
                        } else if (sp6C.x > 1700.0f || sp6C.z > 2800.0f) {
                            effect->mStatus = 1;
                        } else {
                            effect->mStatus = 2;
                        }
                    }
                }
            } else {
                effect->field_0x3c--;
            }
            break;
        }

        sp6C.x = effect->mBasePos.x + effect->mPosition.x;
        sp6C.y = effect->mBasePos.y + effect->mPosition.y;
        sp6C.z = effect->mBasePos.z + effect->mPosition.z;

        if (g_env_light.field_0xea9 != 2) {
            effect->field_0x4c += 600;
            if (effect->field_0x4c > 30000) {
                var_f26 = 0.0f;
            }
        } else {
            var_f26 = 1.0f;
        }

        cLib_addCalc(&effect->mAlpha, var_f26, 0.5f, 0.02f, 0.00001f);
        effect->mAlpha *= var_f31;

        if (var_r27 != 0) {
            effect->mAlpha = 0.0f;
        }

        if (dKy_darkworld_check() == true || var_r24 == 1 || true) {
            f32 var_f1_6 = sp6C.abs(camera->view.lookat.eye);
            effect->field_0x48 = var_f1_6;

            f32 var_f1_7;
            if (var_f1_6 >= 800.0f) {
                var_f1_7 = (var_f1_6 - 800.0f) / 825.0f;
                if (var_f1_7 > 1.0f) {
                    var_f1_7 = 1.0f;
                }
            } else {
                var_f1_7 = 0.0f;
            }

            effect->mAlpha = var_f1_7;
        }

        f32 var_f1_8 = sp6C.abs(camera->view.lookat.eye);
        f32 temp_f25 = var_f1_8 / 2000.0f;
        effect->field_0x48 = 1.0f - (temp_f25 * temp_f25);
#if TARGET_PC
        if (!skipInterpolation) {
            Mtx effectMtx;
            MTXTrans(effectMtx, sp6C.x, sp6C.y, sp6C.z);
            dusk::frame_interp::record_final_mtx(effectMtx, effect);
        }
#endif
    }
}

void draw(Mtx drawMtx, u8** tex, dKankyo_housi_Packet* housi_packet, f32 timeScale) {
    static f32 rot = 0.0f;

    Mtx camMtx;
    Mtx rotMtx;
    cXyz pos[4];
#if TARGET_PC
    static CachedTexObjs<1> texobj;
#else
    TGXTexObj spDC;
#endif
    cXyz spD0;
    Vec spC4;
    Vec spB8;

    bool isPalaceOfTwilight = 0;
#if TARGET_PC
    f32 presentationCounter = g_Counter.mCounter0;
    if (dusk::frame_interp::is_enabled() && !dusk::frame_interp::is_sim_frame()) {
        presentationCounter -= 1.0f - dusk::frame_interp::get_interpolation_step();
    }
    if (true) presentationCounter *= timeScale;
#endif
    if (housi_packet->mHousiCount != 0) {
        if (strcmp(dComIfGp_getStartStageName(), "D_MN08") == 0) {
            isPalaceOfTwilight = 1;
        }

        if (strcmp(dComIfGp_getStartStageName(), "D_MN08") != 0 ||
            dComIfGp_roomControl_getStayNo() == 0 || dComIfGp_roomControl_getStayNo() == 11 ||
            true)
        {
            j3dSys.reinitGX();
            f32 var_f25 = 120.0f;

            if (g_env_light.field_0xea9 == 1) {
                var_f25 = 140.0f;
            } else if (g_env_light.camera_water_in_status != 0 && !true) {
                return;
            }

            GXColor color_reg0;
            color_reg0.r = 0xE5;
            color_reg0.g = 0xFF;
            color_reg0.b = 0xC8;
            color_reg0.a = var_f25;

            GXColor color_reg1;
            color_reg1.r = 0x43;
            color_reg1.g = 0xD2;
            color_reg1.b = 0xCA;
            color_reg1.a = 0xFF;

            if (dKy_darkworld_check() == 1 || isPalaceOfTwilight == 1 || true) {
                color_reg0.r = 0;
                color_reg0.g = 0;
                color_reg0.b = 0;
                color_reg0.a = var_f25;

                color_reg1.r = 0;
                color_reg1.g = 0;
                color_reg1.b = 0;
                color_reg1.a = 0xFF;

                var_f25 = 255.0f;
            } else if (g_env_light.field_0xea9 == 1) {
                color_reg0.r = 0xFF;
                color_reg0.g = 0xFF;
                color_reg0.b = 0xFF;

                color_reg1.r = 0;
                color_reg1.g = 0x50;
                color_reg1.b = 0x50;
            } else if (g_env_light.field_0xea9 == 2 &&
                       (g_env_light.fishing_hole_season == 1 || g_env_light.fishing_hole_season == 3))
            {
                GXColor sp1C = {0x32, 0x32, 0x32, 0xFF};
                GXColor sp18 = {0xFF, 0xD7, 0xF0, 0xFF};

                camera_process_class* cam_p = dComIfGp_getCamera(0);
                if (g_env_light.fishing_hole_season == 3) {
                    sp1C.r = 0x78;
                    sp1C.g = 0x0A;
                    sp1C.b = 0x14;

                    sp18.r = 0x14;
                    sp18.g = 0x3C;
                    sp18.b = 0x00;
                }

                dKy_ParticleColor_get_bg(&cam_p->view.lookat.eye, NULL, &color_reg1, &color_reg0, &sp1C, &sp18,
                                         0.0f);
                var_f25 = 255.0f;
            }

            if (dComIfGd_getView() != NULL) {
                MTXInverse(dComIfGd_getView()->viewMtxNoTrans, camMtx);
            } else {
                return;
            }

            f32 temp_f26 = 1.2f;
            f32 temp_f24 = 6.5f;

            for (int i = 0; i < 1; i++) {
#if TARGET_PC
                load_cached_tex(texobj, (ResTIMG*)*tex, GX_TEXMAP0);
#else
                dKyr_set_btitex(&texobj, (ResTIMG*)*tex);
#endif
#if TARGET_PC
                GXSetNumChans(1);
                GXSetChanCtrl(GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_CLAMP, GX_AF_NONE);
#else
                GXSetNumChans(0);
                GXSetTevColor(GX_TEVREG0, color_reg0);
#endif
                GXSetTevColor(GX_TEVREG1, color_reg1);
                GXSetNumTexGens(1);
                GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
                GXSetNumTevStages(1);
                GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, DUSK_IF_ELSE(GX_COLOR0A0, GX_COLOR_NULL));
                GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, DUSK_IF_ELSE(GX_CC_RASC, GX_CC_C0), GX_CC_TEXC, GX_CC_ZERO);
                GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                                GX_TEVPREV);
                GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, DUSK_IF_ELSE(GX_CA_RASA, GX_CA_CA), GX_CA_TEXA, GX_CA_ZERO);
                GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE,
                                GX_TEVPREV);

                if (strcmp(dComIfGp_getStartStageName(), "F_NW01") == 0 ||
                    g_env_light.field_0xea9 == 1)
                {
                    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_COPY);
                } else {
                    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
                                    GX_LO_SET);
                }

                GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_OR, GX_GREATER, 0);

                if (i == 1) {
                    GXSetZMode(GX_TRUE, GX_GEQUAL, GX_FALSE);
                } else if (true && g_env_light.camera_water_in_status != 0) {
                    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
                } else {
                    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
                }

                GXSetClipMode(GX_CLIP_DISABLE);
                GXSetNumIndStages(0);
                dKr_cullVtx_Set(IF_DUSK(true));

                rot += 1.2f;
                MTXRotRad(rotMtx, 'Z', DEG_TO_RAD(rot));
                MTXConcat(camMtx, rotMtx, camMtx);

                GXLoadPosMtxImm(drawMtx, GX_PNMTX0);
                GXSetCurrentMtx(GX_PNMTX0);

#if TARGET_PC
                // Dusklight optimization: we submit a single large draw call, rather than hundreds.
                u32 vertCount = 4 * housi_packet->mHousiCount;
                GXBegin(GX_QUADS, GX_VTXFMT0, vertCount);
#endif

                for (int j = 0; j < housi_packet->mHousiCount; j++) {
                    fopAc_ac_c* player = dComIfGp_getPlayer(0);

                    spD0.x =
                        housi_packet->mHousiEff[j].mBasePos.x + housi_packet->mHousiEff[j].mPosition.x;
                    spD0.y =
                        housi_packet->mHousiEff[j].mBasePos.y + housi_packet->mHousiEff[j].mPosition.y;
                    spD0.z =
                        housi_packet->mHousiEff[j].mBasePos.z + housi_packet->mHousiEff[j].mPosition.z;
#if TARGET_PC
                    Mtx presentationMtx;
                    if (dusk::frame_interp::lookup_replacement(&housi_packet->mHousiEff[j],
                                                                presentationMtx))
                    {
                        spD0.set(presentationMtx[0][3], presentationMtx[1][3],
                                 presentationMtx[2][3]);
                    }
#endif

                    if (i == 1 && j == 0) {
#if TARGET_PC
                        // Never gets hit I think?
                        abort();
#endif
                        color_reg0.r = 0;
                        color_reg0.g = 0;
                        color_reg0.b = 0;

                        color_reg1.r = 0;
                        color_reg1.g = 0;
                        color_reg1.b = 0;

                        GXSetTevColor(GX_TEVREG1, color_reg1);
                    }

                    if (i == 1) {
                        f32 temp_f4 = 100.0f;
                        if (!(spD0.y > player->current.pos.y + temp_f4)) {
                            if (!(spD0.y < player->current.pos.y - 20.0f)) {
                                if (!(housi_packet->mHousiEff[j].mAlpha <= 0.0f)) {
                                    color_reg0.a =
                                        housi_packet->mHousiEff[j].mAlpha * 40.0f *
                                        (1.0f - ((spD0.y - player->current.pos.y) / 100.0f));
                                    spD0.y = player->current.pos.y - 20.0f;
                                    goto block_14;  // probably fake match
                                }
                            }
                        }
                    } else {
                        color_reg0.a = housi_packet->mHousiEff[j].mAlpha * var_f25;

                    block_14:
#if !TARGET_PC // GXLoadTextObj does nothing, TEV colors replaced with vertex colors
                        GXLoadTexObj(&texobj, GX_TEXMAP0);
                        GXSetTevColor(GX_TEVREG0, color_reg0);
#endif

                        f32 var_f27 = housi_packet->mHousiEff[j].field_0x48 * 9.0f;
                        if (g_env_light.field_0xea9 == 1) {
                            var_f27 = housi_packet->mHousiEff[j].field_0x48 * 18.0f;
                        }

                        f32 temp_f28 =
                            (var_f27 * 0.2f) * cM_fsin(housi_packet->mHousiEff[j].mScale.x * 5.0f);
                        f32 temp_f30 =
                            (var_f27 * 0.2f) * cM_fcos(housi_packet->mHousiEff[j].mScale.y * 6.0f);

                        if (dKy_darkworld_check() == 1 || isPalaceOfTwilight == 1 || true) {
                            cXyz sp7C[] = {
                                cXyz(-1.0f, -0.5f, 0.0f),
                                cXyz(-1.0f, 1.5f, 0.0f),
                                cXyz(1.0f, 1.5f, 0.0f),
                                cXyz(1.0f, -0.5f, 0.0f),
                            };

#if TARGET_PC
                            geometry::transform_particle(sp7C, &spD0, &color_reg0, j, presentationCounter);
#endif

                            for (int k = 0; k < 4; k++) {
                                cXyz spAC;
                                cXyz spA0;

#if TARGET_PC
                                f32 temp_f26_2 =
                                    cM_ssin((f32)j * 123.0f + presentationCounter * 600.0f);
#else
                                f32 temp_f26_2 = cM_ssin(
                                    (f32)j * 123.0f + (f32)(g_Counter.mCounter0 * 600));
#endif

                                cXyz* temp_r3 = &sp7C[k];
                                spAC.x = temp_r3->x * (8.0f * (1.0f + (temp_f26_2 * 0.3f)));
                                spAC.y = temp_r3->y * (8.0f * (1.0f + (temp_f26_2 * 0.3f)));
                                spAC.z = temp_r3->z * (8.0f * (1.0f + (temp_f26_2 * 0.3f)));

                                mDoMtx_stack_c::transS(spD0.x, spD0.y, spD0.z);
                                mDoMtx_stack_c::YrotM(temp_f26_2 * 65536.0f);
                                mDoMtx_stack_c::multVec(&spAC, &spA0);
                                pos[k] = spA0;
                            }
                        } else if (g_env_light.field_0xea9 == 2) {
                            cXyz sp4C[] = {
                                cXyz(-1.0f, -0.9f, 0.0f),
                                cXyz(-1.0f, 1.1f, 0.0f),
                                cXyz(1.0f, 1.1f, 0.0f),
                                cXyz(1.0f, -0.9f, 0.0f),
                            };

                            for (int k = 0; k < 4; k++) {
                                cXyz sp94;
                                cXyz sp88;

                                f32 var_f24;
                                if (housi_packet->mHousiEff[j].mStatus == 1 ||
                                    housi_packet->mHousiEff[j].mStatus == 3)
                                {
                                    var_f24 =
                                        0.2f +
                                        (housi_packet->mHousiEff[j].field_0x34 *
                                            (fabsf(cM_ssin((f32)j * 213.0f +
                                                        (f32)(g_Counter.mCounter0 * 330))) *
                                            0.8f));
                                } else {
                                    var_f24 = cM_ssin((f32)j * 123.0f +
                                                        (f32)(g_Counter.mCounter0 * 80));
                                }

                                f32 var_f2;
                                if (g_env_light.fishing_hole_season == 3) {
                                    var_f2 = 15.0f;

                                    if (housi_packet->mHousiEff[j].mStatus == 1) {
                                        var_f24 =
                                            housi_packet->mHousiEff[j].field_0x34 *
                                            fabsf(cM_ssin((f32)j * 250.0f +
                                                            (f32)(g_Counter.mCounter0 * 88)));
                                    } else {
                                        var_f24 = cM_ssin((f32)j * 685.0f +
                                                            (f32)(g_Counter.mCounter0 * 20));
                                    }
                                } else {
                                    var_f2 = 6.0f;
                                }

                                cXyz* temp_r3_2 = &sp4C[k];
                                sp94.x = temp_r3_2->x * (var_f2 * (1.0f + (var_f24 * 0.3f)));
                                sp94.y = temp_r3_2->y * (var_f2 * (1.0f + (var_f24 * 0.3f)));
                                sp94.z = temp_r3_2->z * (var_f2 * (1.0f + (var_f24 * 0.3f)));
                                mDoMtx_stack_c::transS(spD0.x, spD0.y, spD0.z);

                                if (housi_packet->mHousiEff[j].mStatus == 1 ||
                                    housi_packet->mHousiEff[j].mStatus == 3)
                                {
                                    housi_packet->mHousiEff[j].field_0x38 +=
                                        483.0f * (0.5f + (var_f24 * 0.5f));

                                    housi_packet->mHousiEff[j].field_0x44 =
                                        (s16)housi_packet->mHousiEff[j].field_0x38;
                                    mDoMtx_stack_c::YrotM(housi_packet->mHousiEff[j].field_0x38);
                                    mDoMtx_stack_c::XrotM(housi_packet->mHousiEff[j].field_0x38);
                                    mDoMtx_stack_c::ZrotM(housi_packet->mHousiEff[j].field_0x38);
                                } else {
                                    if (housi_packet->mHousiEff[j].mStatus == 2) {
                                        if (g_env_light.fishing_hole_season == 3) {
                                            housi_packet->mHousiEff[j].field_0x38 += var_f24 * 30.0f;
                                        } else {
                                            housi_packet->mHousiEff[j].field_0x38 +=
                                                var_f24 * 100.0f;
                                        }
                                    }

                                    if (housi_packet->mHousiEff[j].field_0x38 > 32765.0f) {
                                        cLib_addCalc(&housi_packet->mHousiEff[j].field_0x44,
                                                        -16384.0f, 0.1f, 500.0f, 0.0001f);
                                    } else {
                                        cLib_addCalc(&housi_packet->mHousiEff[j].field_0x44,
                                                        16384.0f, 0.1f, 500.0f, 0.0001f);
                                    }

                                    mDoMtx_stack_c::YrotM(housi_packet->mHousiEff[j].field_0x38);
                                    mDoMtx_stack_c::XrotM(housi_packet->mHousiEff[j].field_0x44);
                                    mDoMtx_stack_c::ZrotM(housi_packet->mHousiEff[j].field_0x38);
                                }

                                mDoMtx_stack_c::multVec(&sp94, &sp88);
                                pos[k] = sp88;
                            }
                        } else {
                            spC4.x = var_f27 - temp_f30;
                            spC4.y = var_f27 - temp_f28;
                            spC4.z = 0.0f;
                            MTXMultVec(camMtx, &spC4, &spB8);
                            pos[0].x = spD0.x + spB8.x;
                            pos[0].y = spD0.y + spB8.y;
                            pos[0].z = spD0.z + spB8.z;

                            spC4.x = -var_f27 + temp_f30;
                            spC4.y = var_f27 - temp_f28;
                            spC4.z = 0.0f;
                            MTXMultVec(camMtx, &spC4, &spB8);
                            pos[1].x = spD0.x + spB8.x;
                            pos[1].y = spD0.y + spB8.y;
                            pos[1].z = spD0.z + spB8.z;

                            spC4.x = -var_f27 + temp_f30;
                            spC4.y = -var_f27 + temp_f28;
                            spC4.z = 0.0f;
                            MTXMultVec(camMtx, &spC4, &spB8);
                            pos[2].x = spD0.x + spB8.x;
                            pos[2].y = spD0.y + spB8.y;
                            pos[2].z = spD0.z + spB8.z;

                            spC4.x = var_f27 - temp_f30;
                            spC4.y = -var_f27 + temp_f28;
                            spC4.z = 0.0f;
                            MTXMultVec(camMtx, &spC4, &spB8);
                            pos[3].x = spD0.x + spB8.x;
                            pos[3].y = spD0.y + spB8.y;
                            pos[3].z = spD0.z + spB8.z;
                        }

                        IF_NOT_DUSK(GXBegin(GX_QUADS, GX_VTXFMT0, 4));

                        s16 var_r17 = 0x1FF;
                        if (dKy_darkworld_check() == true || isPalaceOfTwilight == 1 || true) {
                            var_r17 = 0xFA;
                        }

                        GXPosition3f32(pos[0].x, pos[0].y, pos[0].z);
                        IF_DUSK(GXColor4u8(color_reg0.r, color_reg0.g, color_reg0.b, color_reg0.a));
                        GXTexCoord2s16(0, 0);
                        GXPosition3f32(pos[1].x, pos[1].y, pos[1].z);
                        IF_DUSK(GXColor4u8(color_reg0.r, color_reg0.g, color_reg0.b, color_reg0.a));
                        GXTexCoord2s16(var_r17, 0);
                        GXPosition3f32(pos[2].x, pos[2].y, pos[2].z);
                        IF_DUSK(GXColor4u8(color_reg0.r, color_reg0.g, color_reg0.b, color_reg0.a));
                        GXTexCoord2s16(var_r17, var_r17);
                        GXPosition3f32(pos[3].x, pos[3].y, pos[3].z);
                        IF_DUSK(GXColor4u8(color_reg0.r, color_reg0.g, color_reg0.b, color_reg0.a));
                        GXTexCoord2s16(0, var_r17);


                        IF_NOT_DUSK(GXEnd());
                    }
                }

#if TARGET_PC
                GXEnd();
#endif
            }

            GXSetClipMode(GX_CLIP_ENABLE);
            J3DShape::resetVcdVatCache();
        }
    }
}


}
