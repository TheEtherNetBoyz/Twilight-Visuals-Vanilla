#include "settings.hpp"
#include "runtime.hpp"
#include "hooks.hpp"
#include "environment.hpp"
#include "postprocess.hpp"
#include "particles.hpp"
#include "music.hpp"
#include "geometry.hpp"
#include "boundary.hpp"
#include "running.hpp"
#include "wall_run.hpp"
#include "cursor.hpp"
#include "sequencing.hpp"
#include "sky.hpp"
#include "menu_scaling.hpp"
#include "facial.hpp"
#include "native_face_tuner.hpp"
#include "load_acceleration.hpp"
#include <cstdio>

#include "mods/service.hpp"
#include "mods/svc/audio_res.h"
#include "mods/svc/hook.h"
#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/overlay.h"
#include "mods/svc/resource.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(HostService, svc_host);
IMPORT_SERVICE(AudioResService, svc_audio_res);
IMPORT_SERVICE(OverlayService, svc_overlay);

const HookService* twilight_hook_service() { return svc_hook; }
const LogService* twilight_log_service() { return svc_log; }
const HostService* twilight_host_service() { return svc_host; }
const AudioResService* twilight_audio_res_service() { return svc_audio_res; }
const OverlayService* twilight_overlay_service() { return svc_overlay; }

extern "C" {
MOD_EXPORT ModResult mod_shutdown(ModError*);

MOD_EXPORT ModResult mod_initialize(ModError* error) {
    ModResult result = twilight_visuals::register_settings(error);
    if (result != MOD_OK) return result;

    result = twilight_visuals::register_quick_menu_tab(error);
    if (result != MOD_OK) return result;

    twilight_visuals::refresh_runtime_settings();

    twilight_visuals::geometry::initialize();
    twilight_visuals::boundary::initialize();
    twilight_visuals::running::initialize();
    twilight_visuals::wall_run::initialize();
    twilight_visuals::sequencing::initialize();
    result = twilight_visuals::menu_scaling::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Menu scaling hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::cursor::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Mouse cursor hook unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::facial::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Facial animation hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::native_face_tuner::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Native facial tuner unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::load_acceleration::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Load acceleration hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::music::initialize();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Astral music resources unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Speedrun integration hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::environment::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Environment hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::postprocess::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Post-processing hook unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    result = twilight_visuals::particles::install_hooks();
    if (result != MOD_OK) {
        if (error) {
            error->code = result;
            std::snprintf(error->message, sizeof(error->message), "Particle hooks unavailable");
        }
        mod_shutdown(nullptr);
        return result;
    }

    svc_log->info(mod_ctx, "Twilight Visuals mod initialized");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    twilight_visuals::refresh_runtime_settings();
    twilight_visuals::facial::update();
    twilight_visuals::music::update();
    twilight_visuals::load_acceleration::update();
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    twilight_visuals::load_acceleration::uninstall_hooks();
    twilight_visuals::native_face_tuner::shutdown();
    twilight_visuals::facial::shutdown();
    twilight_visuals::cursor::shutdown();
    twilight_visuals::sky::shutdown();
    twilight_visuals::wall_run::shutdown();
    twilight_visuals::running::shutdown();
    twilight_visuals::sequencing::shutdown();
    twilight_visuals::menu_scaling::uninstall_hooks();
    twilight_visuals::geometry::shutdown();
    twilight_visuals::close_settings_window();
    twilight_visuals::unregister_quick_menu_tab();
    twilight_visuals::particles::uninstall_hooks();
    twilight_visuals::postprocess::uninstall_hooks();
    twilight_visuals::environment::uninstall_hooks();
    twilight_visuals::boundary::shutdown();
    twilight_visuals::uninstall_hooks();
    twilight_visuals::music::shutdown();
    svc_log->info(mod_ctx, "Twilight Visuals mod unloaded");
    return MOD_OK;
}

}
