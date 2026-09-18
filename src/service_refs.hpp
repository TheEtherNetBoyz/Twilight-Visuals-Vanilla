#pragma once

#include "mods/svc/audio_res.h"
#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/overlay.h"

// Imported once in mod.cpp, referenced by feature translation units.
const LogService* twilight_log_service();
const HostService* twilight_host_service();
const AudioResService* twilight_audio_res_service();
const OverlayService* twilight_overlay_service();
#define svc_log (twilight_log_service())
#define svc_host (twilight_host_service())
#define svc_audio_res (twilight_audio_res_service())
#define svc_overlay (twilight_overlay_service())
