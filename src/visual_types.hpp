#pragma once
#include "dolphin/types.h"

// Mod-local data. This deliberately contains no Dusklight host ABI types.
struct VisualRgb { u8 r; u8 g; u8 b; };
struct VisualRgba { u8 r; u8 g; u8 b; u8 a; };
struct VisualSkybox {
    VisualRgb sky;
    VisualRgb cloudTop;
    VisualRgb cloudBottom;
    VisualRgba cloudShadow;
    VisualRgba hazeOuter;
    VisualRgba hazeInner;
};
