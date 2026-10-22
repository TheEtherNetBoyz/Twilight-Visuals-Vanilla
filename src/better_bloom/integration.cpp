#include "integration.hpp"
#include "bloom.hpp"
#include "pulse.hpp"
#include "renderer.hpp"
#include "../settings.hpp"

#include "d/d_kankyo.h"
#include "m_Do/m_Do_graphic.h"
#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/log.h"
#include "mods/svc/resource.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <string_view>

IMPORT_SERVICE_VERSION(GfxService, svc_gfx, 2);
IMPORT_SERVICE(ResourceService, svc_bloom_resource);
IMPORT_SERVICE(LogService, svc_bloom_log);

namespace twilight_visuals::better_bloom {
namespace {
bloom::Renderer renderer;
GfxDrawTypeHandle drawType{};
GfxComputeTypeHandle computeType{};
uint64_t serial{};
std::atomic_bool renderFailed{};
bool failureReported{};
bloom::Pulse pulse;
std::chrono::steady_clock::time_point lastPulseSample;
bool pulseSampled{};

class Resource {
  public:
    explicit Resource(const char* name) { svc_bloom_resource->load(mod_ctx, name, &buffer); }
    ~Resource() { svc_bloom_resource->free(mod_ctx, &buffer); }
    std::string_view text() const {
        return buffer.data == nullptr ? std::string_view{} :
            std::string_view(static_cast<const char*>(buffer.data), buffer.size);
    }
  private:
    ResourceBuffer buffer = RESOURCE_BUFFER_INIT;
};

float percent(ConfigVarHandle handle, int64_t fallback, int64_t maximum) {
    return static_cast<float>(std::clamp<int64_t>(get_int(handle, fallback), 0, maximum)) / 100.0f;
}

std::array<float, 4> colour(GXColor value) {
    return {value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f};
}

void on_compute(ModContext*, const GfxComputeContext* context, const void* payload, size_t size,
                void*) {
    if (size != sizeof(bloom::Frame) || renderFailed.load(std::memory_order_relaxed)) return;
    bloom::Frame frame;
    std::memcpy(&frame, payload, sizeof(frame));
    try {
        if (!renderer.compute(*context, frame)) renderFailed.store(true, std::memory_order_release);
    } catch (...) { renderFailed.store(true, std::memory_order_release); }
}

void on_draw(ModContext*, const GfxDrawContext* context, const void* payload, size_t size, void*) {
    if (size != sizeof(bloom::Frame) || renderFailed.load(std::memory_order_relaxed)) return;
    bloom::Frame frame;
    std::memcpy(&frame, payload, sizeof(frame));
    try {
        if (!renderer.composite(*context, frame)) renderFailed.store(true, std::memory_order_release);
    } catch (...) { renderFailed.store(true, std::memory_order_release); }
}
}

ModResult initialize(ModError* error) {
    renderFailed.store(false, std::memory_order_relaxed);
    failureReported = false;
    pulse.reset();
    pulseSampled = false;
    serial = 0;
    GfxDeviceInfo device = GFX_DEVICE_INFO_INIT;
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (svc_gfx->get_device_info(mod_ctx, &device) != MOD_OK || device.device == nullptr ||
        svc_gfx->get_scene_target_layout(mod_ctx, &layout) != MOD_OK)
        return mods::set_error(error, MOD_UNAVAILABLE, "Better Bloom graphics device unavailable.");
    Resource filter("better_bloom_filter.wgsl");
    Resource composite("better_bloom_composite.wgsl");
    if (filter.text().empty() || composite.text().empty())
        return mods::set_error(error, MOD_ERROR, "Better Bloom shaders could not be loaded.");
    if (!renderer.initialize(device.device, filter.text(), composite.text(), layout))
        return mods::set_error(error, MOD_UNSUPPORTED, "Better Bloom pipelines could not be created.");
    GfxComputeTypeDesc compute = GFX_COMPUTE_TYPE_DESC_INIT;
    compute.label = "Twilight Visuals Better Bloom";
    compute.callback = on_compute;
    ModResult result = svc_gfx->register_compute_type(mod_ctx, &compute, &computeType);
    if (result != MOD_OK) return mods::set_error(error, result, "Could not register Better Bloom filter.");
    GfxDrawTypeDesc drawDesc = GFX_DRAW_TYPE_DESC_INIT;
    drawDesc.label = "Twilight Visuals Better Bloom composite";
    drawDesc.draw = on_draw;
    result = svc_gfx->register_draw_type(mod_ctx, &drawDesc, &drawType);
    if (result != MOD_OK) return mods::set_error(error, result, "Could not register Better Bloom composite.");
    return MOD_OK;
}

HookAction draw(void* bloomObject) {
    auto* current = static_cast<mDoGph_gInf_c::bloom_c*>(bloomObject);
    const auto& config = settings();
    if (current == nullptr || !get_bool(config.bloomEnabled, true) ||
        renderFailed.load(std::memory_order_acquire)) {
        pulse.reset(); pulseSampled = false; return HOOK_CONTINUE;
    }
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = pulseSampled ? std::chrono::duration<double>(now - lastPulseSample).count() : 0;
    lastPulseSample = now; pulseSampled = true;
    bloom::PulseSettings pulseSettings;
    pulseSettings.enabled = get_bool(config.bloomPulseEnabled, true);
    pulseSettings.source = get_int(config.bloomPulseActivation, 0) == 0 ?
        bloom::PulseSource::Vanilla : bloom::PulseSource::Everywhere;
    pulseSettings.period = std::clamp<int64_t>(get_int(config.bloomPulsePeriod, 1457), 100, 60000) / 1000.0;
    pulseSettings.irregular = get_bool(config.bloomPulseIrregular, true);
    pulseSettings.sizeAmount = percent(config.bloomPulseSize, 20, 100);
    pulseSettings.intensityAmount = percent(config.bloomPulseIntensity, 0, 100);
    pulseSettings.fade = std::clamp<int64_t>(get_int(config.bloomPulseTransition, 500), 0, 10000) / 1000.0;
    const bloom::PulseScale animation = pulse.advance(elapsed, dKy_darkworld_check() != 0, pulseSettings);
    const bool enabled = current->getEnable() || get_int(config.bloomActivation, 0) != 0;
    if (!enabled && current->getMonoColor()->a == 0) return HOOK_SKIP_ORIGINAL;
    GfxResolveDesc request = GFX_RESOLVE_DESC_INIT;
    GfxResolvedTargets scene = GFX_RESOLVED_TARGETS_INIT;
    if (svc_gfx->resolve_pass(mod_ctx, &request, &scene) != MOD_OK || scene.color == nullptr ||
        scene.width == 0 || scene.height == 0) return HOOK_CONTINUE;

    bloom::FrameUniforms uniforms;
    std::array<float, 3> tint{percent(config.bloomTintRed, 100, 200),
        percent(config.bloomTintGreen, 100, 200), percent(config.bloomTintBlue, 100, 200)};
    uniforms.tint = bloom::resolve_tint(colour(*current->getBlendColor()), tint,
        get_int(config.bloomTintSource, 0) != 0, percent(config.bloomSceneDarkening, 100, 100));
    uniforms.mono = colour(*current->getMonoColor());
    uniforms.mono[3] = std::clamp(uniforms.mono[3] * percent(config.bloomColourLayerStrength, 100, 1000), 0.0f, 1.0f);
    uniforms.threshold = bloom::resolve_threshold(current->getPoint() / 255.0f,
        percent(config.bloomOverrideThreshold, 100, 400),
        static_cast<bloom::ThresholdSource>(std::clamp<int64_t>(get_int(config.bloomThresholdSource), 0, 2)),
        percent(config.bloomThresholdNormalization, 50, 100));
    // Better Bloom exposes softness as a percentage, then scales it to the
    // shader's 0.0-4.0 knee range.
    uniforms.knee = percent(config.bloomThreshold, 50, 667) * 0.6f;
    const bool fixedGain = get_int(config.bloomGainSource, 0) != 0;
    uniforms.gain = 8.0f * (fixedGain ? 128.0f / 255.0f : current->getBlureRatio() / 255.0f);
    const float responseGain = percent(config.bloomResponseGain, 100, 4000) * bloom::defaultResponseGain;
    const float rolloff = percent(config.bloomHighlightRolloff, 100, 400);
    const float compensation = get_bool(config.bloomAutoCompensation, true) ?
        bloom::response_compensation(responseGain, rolloff) : 1.0f;
    // Keep Better Bloom independent from Twilight Visuals' legacy bloom
    // brightness control. The source mod applies only its own intensity here.
    uniforms.intensity = percent(config.betterBloomIntensity, 100, 20000) *
        compensation * animation.intensity;
    uniforms.saturation = percent(config.bloomSaturation, 100, 300);
    uniforms.responseGain = responseGain;
    uniforms.highlightColour = percent(config.bloomHighlightDesaturation, 0, 100);
    uniforms.highlightRolloff = rolloff;
    uniforms.highlightLimit = percent(config.bloomHighlightLimit, 0, 4000);
    uniforms.brightnessMetric = std::clamp<int64_t>(get_int(config.bloomBrightnessMetric), 0, 3);
    uniforms.preDiffusionGain = percent(config.bloomPreDiffusionGain, 0, 100);
    uniforms.sourceLimit = percent(config.bloomSourceLimit, 0, 4000);
    uniforms.filteringSpace = std::clamp<int64_t>(get_int(config.bloomFilteringSpace), 0, 1);
    uniforms.spread = {percent(config.bloomHorizontalSpread, 100, 400),
        percent(config.bloomVerticalSpread, 100, 400)};
    const int64_t blend = std::clamp<int64_t>(get_int(config.bloomBlendMode), 0, 2);
    uniforms.mode = blend == 0 ? current->mMode : static_cast<uint32_t>(blend - 1);
    uniforms.enabled = enabled ? 1u : 0u;
    uniforms.sourceSrgb = bloom::is_srgb(scene.color_format) ? 1u : 0u;
    uniforms.targetSrgb = uniforms.sourceSrgb;

    const float radius = 0.005f * percent(config.bloomSize, 100, 4000) * animation.size;
    const std::array<float, 4> widths{percent(config.bloomCoreWidth, 100, 5000),
        percent(config.bloomInnerWidth, 100, 5000), percent(config.bloomOuterWidth, 100, 5000),
        percent(config.bloomHaloWidth, 100, 5000)};
    const std::array<float, 4> contributions{percent(config.bloomCoreContribution, 100, 2000),
        percent(config.bloomInnerContribution, 100, 2000), percent(config.bloomOuterContribution, 100, 2000),
        percent(config.bloomHaloContribution, 100, 2000)};
    const bloom::Profile profile = bloom::make_profile(widths, contributions,
        percent(config.bloomDiffusion, 0, 4000) * 1.7f);
    const uint32_t divisor = 1u << std::clamp<int64_t>(get_int(config.bloomResolution, 1), 0, 2);
    const bloom::Plan plan = bloom::make_plan(scene.width, scene.height, radius, profile, divisor);
    const bloom::UniformBlock block = bloom::pack_uniforms(uniforms, plan);
    GfxRange range{};
    if (svc_gfx->push_uniform(mod_ctx, block.data(), bloom::uniformAlignment * (plan.count + 1), &range) != MOD_OK)
        return HOOK_CONTINUE;
    bloom::Frame frame{scene.color, ++serial, range.offset, scene.width, scene.height, radius};
    frame.divisor = divisor; frame.filter = enabled && uniforms.intensity > 0 && uniforms.gain > 0 ? 1u : 0u;
    frame.profile = profile;
    if (svc_gfx->push_compute(mod_ctx, computeType, &frame, sizeof(frame)) != MOD_OK ||
        svc_gfx->push_draw(mod_ctx, drawType, &frame, sizeof(frame)) != MOD_OK) return HOOK_CONTINUE;
    return HOOK_SKIP_ORIGINAL;
}

void update() {
    if (renderFailed.load(std::memory_order_acquire) && !failureReported) {
        svc_bloom_log->error(mod_ctx, "Better Bloom rendering failed; restored the original bloom path.");
        failureReported = true;
    }
}

void shutdown() {
    renderer.reset(); drawType = 0; computeType = 0; pulse.reset(); pulseSampled = false;
}
}
