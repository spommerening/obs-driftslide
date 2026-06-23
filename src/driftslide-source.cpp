#include "driftslide-source.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <graphics/graphics.h>
#include <graphics/image-file.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <random>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void unload_texture(DriftSlideSource *ctx)
{
	if (!ctx->texture_loaded)
		return;
	obs_enter_graphics();
	gs_image_file4_free(&ctx->if4);
	memset(&ctx->if4, 0, sizeof(ctx->if4));
	ctx->texture_loaded = false;
	ctx->img_width = 0;
	ctx->img_height = 0;
	obs_leave_graphics();
}

static void load_next_image(DriftSlideSource *ctx)
{
	if (!ctx->image_list || ctx->image_list->size() == 0)
		return;

	std::string path = ctx->image_list->next();
	if (path.empty())
		return;

	obs_enter_graphics();

	if (ctx->texture_loaded) {
		gs_image_file4_free(&ctx->if4);
		memset(&ctx->if4, 0, sizeof(ctx->if4));
		ctx->texture_loaded = false;
	}

	gs_image_file4_init(&ctx->if4, path.c_str(), GS_IMAGE_ALPHA_PREMULTIPLY_SRGB);
	gs_image_file4_init_texture(&ctx->if4);

	gs_texture_t *tex = ctx->if4.image3.image2.image.texture;
	if (tex) {
		ctx->img_width = gs_texture_get_width(tex);
		ctx->img_height = gs_texture_get_height(tex);
		ctx->texture_loaded = true;
	} else {
		obs_log(LOG_WARNING, "Failed to load texture: %s", path.c_str());
		memset(&ctx->if4, 0, sizeof(ctx->if4));
	}

	obs_leave_graphics();

	// Randomise Ken Burns parameters for this image's visible lifetime
	static std::mt19937 rng{std::random_device{}()};
	std::uniform_real_distribution<float> pan_dist(-0.03f, 0.03f);
	std::uniform_int_distribution<int> zoom_coin(0, 1);
	if (zoom_coin(rng)) {
		ctx->kb_start_zoom = 1.0f;
		ctx->kb_end_zoom = 1.1f;
	} else {
		ctx->kb_start_zoom = 1.1f;
		ctx->kb_end_zoom = 1.0f;
	}
	ctx->kb_start_pan_x = pan_dist(rng);
	ctx->kb_start_pan_y = pan_dist(rng);
	ctx->kb_end_pan_x = pan_dist(rng);
	ctx->kb_end_pan_y = pan_dist(rng);
	ctx->kb_elapsed = 0.0f;
	ctx->kb_total_dur = 2.0f * ctx->transition_dur + ctx->display_dur;
}

static float compute_t(DriftSlideSource *ctx)
{
	if (ctx->state == DSState::Displaying)
		return 1.0f;

	float dur = ctx->transition_dur;
	if (dur <= 0.001f)
		return (ctx->state == DSState::FadeIn) ? 1.0f : 0.0f;

	float raw = ctx->state_timer / dur;
	raw = std::clamp(raw, 0.0f, 1.0f);
	float smooth = raw * raw * (3.0f - 2.0f * raw);
	return (ctx->state == DSState::FadeOut) ? (1.0f - smooth) : smooth;
}

// ---------------------------------------------------------------------------
// OBS callbacks
// ---------------------------------------------------------------------------

void *driftslide_create(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new DriftSlideSource();
	ctx->source = source;

	obs_source_set_flags(source, 0);

	obs_enter_graphics();
	char *effect_path = obs_module_file("effects/driftslide.effect");
	ctx->slide_effect = gs_effect_create_from_file(effect_path, nullptr);
	bfree(effect_path);
	obs_leave_graphics();

	if (!ctx->slide_effect)
		obs_log(LOG_WARNING, "driftslide: failed to load effect");

	// Apply initial settings synchronously (render thread not yet running)
	ctx->directory = obs_data_get_string(settings, "image_directory");
	ctx->random_order = (obs_data_get_int(settings, "image_order") == 1);
	ctx->transparent_dur = std::max((float)obs_data_get_double(settings, "transparent_duration"), 0.1f);
	ctx->display_dur = std::max((float)obs_data_get_double(settings, "display_duration"), 0.1f);
	ctx->transition_dur = std::max((float)obs_data_get_double(settings, "transition_duration"), 0.0f);
	ctx->transition_type = static_cast<TransitionType>(obs_data_get_int(settings, "transition_type"));
	ctx->ken_burns_enabled = obs_data_get_bool(settings, "ken_burns");

	ctx->image_list = std::make_unique<ImageList>(ctx->directory, ctx->random_order);
	obs_log(LOG_INFO, "driftslide: scanned '%s', found %d image(s)", ctx->directory.c_str(),
		ctx->image_list->size());

	// Mirror to pending so the first update() sees no change
	ctx->pending_dir = ctx->directory;
	ctx->pending_random = ctx->random_order;
	ctx->pending_tdur = ctx->transparent_dur;
	ctx->pending_ddur = ctx->display_dur;
	ctx->pending_trdur = ctx->transition_dur;
	ctx->pending_ttype = ctx->transition_type;
	ctx->pending_ken_burns = ctx->ken_burns_enabled;

	return ctx;
}

void driftslide_destroy(void *data)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);

	obs_enter_graphics();
	if (ctx->texture_loaded)
		gs_image_file4_free(&ctx->if4);
	if (ctx->slide_effect)
		gs_effect_destroy(ctx->slide_effect);
	obs_leave_graphics();

	delete ctx;
}

void driftslide_update(void *data, obs_data_t *settings)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);

	std::lock_guard<std::mutex> lock(ctx->settings_mutex);
	ctx->pending_dir = obs_data_get_string(settings, "image_directory");
	ctx->pending_random = (obs_data_get_int(settings, "image_order") == 1);
	ctx->pending_tdur = (float)obs_data_get_double(settings, "transparent_duration");
	ctx->pending_ddur = (float)obs_data_get_double(settings, "display_duration");
	ctx->pending_trdur = (float)obs_data_get_double(settings, "transition_duration");
	ctx->pending_ttype = static_cast<TransitionType>(obs_data_get_int(settings, "transition_type"));
	ctx->pending_ken_burns = obs_data_get_bool(settings, "ken_burns");
	ctx->settings_dirty = true;
}

void driftslide_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "image_directory", "");
	obs_data_set_default_int(settings, "image_order", 0);
	obs_data_set_default_double(settings, "transparent_duration", 30.0);
	obs_data_set_default_double(settings, "display_duration", 15.0);
	obs_data_set_default_double(settings, "transition_duration", 2.0);
	obs_data_set_default_int(settings, "transition_type", 0);
	obs_data_set_default_bool(settings, "ken_burns", false);
}

obs_properties_t *driftslide_get_properties(void * /*data*/)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, "image_directory", obs_module_text("Directory"), OBS_PATH_DIRECTORY, nullptr,
				nullptr);

	obs_property_t *order = obs_properties_add_list(props, "image_order", obs_module_text("ImageOrder"),
							OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(order, obs_module_text("Alphabetic"), 0);
	obs_property_list_add_int(order, obs_module_text("Random"), 1);

	obs_properties_add_float_slider(props, "transparent_duration", obs_module_text("TransparentDuration"), 1.0,
					3600.0, 1.0);

	obs_properties_add_float_slider(props, "display_duration", obs_module_text("DisplayDuration"), 1.0, 300.0, 1.0);

	obs_properties_add_float_slider(props, "transition_duration", obs_module_text("TransitionDuration"), 0.1, 10.0,
					0.1);

	obs_property_t *tt = obs_properties_add_list(props, "transition_type", obs_module_text("TransitionType"),
						     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(tt, obs_module_text("Fade"), 0);
	obs_property_list_add_int(tt, obs_module_text("SlideLeft"), 1);
	obs_property_list_add_int(tt, obs_module_text("SlideRight"), 2);
	obs_property_list_add_int(tt, obs_module_text("SlideUp"), 3);
	obs_property_list_add_int(tt, obs_module_text("SlideDown"), 4);
	obs_property_list_add_int(tt, obs_module_text("Zoom"), 5);
	obs_property_list_add_int(tt, obs_module_text("ScrollLeft"), 6);
	obs_property_list_add_int(tt, obs_module_text("ScrollRight"), 7);
	obs_property_list_add_int(tt, obs_module_text("ScrollUp"), 8);
	obs_property_list_add_int(tt, obs_module_text("ScrollDown"), 9);
	obs_property_list_add_int(tt, obs_module_text("ZoomIn"), 10);

	obs_properties_add_bool(props, "ken_burns", obs_module_text("KenBurns"));

	return props;
}

void driftslide_video_tick(void *data, float seconds)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);

	// Apply pending settings from UI thread
	{
		std::lock_guard<std::mutex> lock(ctx->settings_mutex);
		if (ctx->settings_dirty) {
			bool dir_changed = (ctx->directory != ctx->pending_dir) ||
					   (ctx->random_order != ctx->pending_random);

			ctx->directory = ctx->pending_dir;
			ctx->random_order = ctx->pending_random;
			ctx->transparent_dur = std::max(ctx->pending_tdur, 0.1f);
			ctx->display_dur = std::max(ctx->pending_ddur, 0.1f);
			ctx->transition_dur = std::max(ctx->pending_trdur, 0.0f);
			ctx->transition_type = ctx->pending_ttype;
			ctx->ken_burns_enabled = ctx->pending_ken_burns;
			ctx->settings_dirty = false;

			if (dir_changed) {
				unload_texture(ctx);
				ctx->state = DSState::Transparent;
				ctx->state_timer = 0.0f;
				ctx->image_list = std::make_unique<ImageList>(ctx->directory, ctx->random_order);
				obs_log(LOG_INFO, "driftslide: scanned '%s', found %d image(s)", ctx->directory.c_str(),
					ctx->image_list->size());
			}
		}
	}

	ctx->state_timer += seconds;
	if (ctx->texture_loaded)
		ctx->kb_elapsed = std::min(ctx->kb_elapsed + seconds, ctx->kb_total_dur);

	switch (ctx->state) {
	case DSState::Transparent:
		if (ctx->state_timer >= ctx->transparent_dur) {
			if (!ctx->image_list || ctx->image_list->size() == 0) {
				// No images: clamp timer to avoid float overflow
				ctx->state_timer = ctx->transparent_dur;
				break;
			}
			ctx->state_timer = 0.0f;
			load_next_image(ctx);
			if (ctx->texture_loaded)
				ctx->state = DSState::FadeIn;
		}
		break;

	case DSState::FadeIn:
		if (ctx->state_timer >= ctx->transition_dur) {
			ctx->state_timer = 0.0f;
			ctx->state = DSState::Displaying;
		}
		break;

	case DSState::Displaying:
		if (ctx->state_timer >= ctx->display_dur) {
			ctx->state_timer = 0.0f;
			ctx->state = DSState::FadeOut;
		}
		break;

	case DSState::FadeOut:
		if (ctx->state_timer >= ctx->transition_dur) {
			ctx->state_timer = 0.0f;
			unload_texture(ctx);
			ctx->state = DSState::Transparent;
		}
		break;
	}
}

void driftslide_video_render(void *data, gs_effect_t * /*effect*/)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);

	if (ctx->state == DSState::Transparent || !ctx->texture_loaded)
		return;

	gs_texture_t *tex = ctx->if4.image3.image2.image.texture;
	if (!tex)
		return;

	gs_effect_t *eff = ctx->slide_effect;
	if (!eff)
		return;

	float t = compute_t(ctx);

	gs_eparam_t *p_image = gs_effect_get_param_by_name(eff, "image");
	gs_eparam_t *p_t = gs_effect_get_param_by_name(eff, "t");
	gs_eparam_t *p_tt = gs_effect_get_param_by_name(eff, "transition_type");
	gs_eparam_t *p_fo = gs_effect_get_param_by_name(eff, "is_fading_out");
	gs_eparam_t *p_kb_zoom = gs_effect_get_param_by_name(eff, "kb_zoom");
	gs_eparam_t *p_kb_pan_x = gs_effect_get_param_by_name(eff, "kb_pan_x");
	gs_eparam_t *p_kb_pan_y = gs_effect_get_param_by_name(eff, "kb_pan_y");

	float kb_zoom_val = 1.0f;
	float kb_pan_x_val = 0.0f;
	float kb_pan_y_val = 0.0f;
	if (ctx->ken_burns_enabled && ctx->kb_total_dur > 0.001f) {
		float raw = std::clamp(ctx->kb_elapsed / ctx->kb_total_dur, 0.0f, 1.0f);
		float smooth = raw * raw * (3.0f - 2.0f * raw);
		kb_zoom_val = ctx->kb_start_zoom + (ctx->kb_end_zoom - ctx->kb_start_zoom) * smooth;
		kb_pan_x_val = ctx->kb_start_pan_x + (ctx->kb_end_pan_x - ctx->kb_start_pan_x) * smooth;
		kb_pan_y_val = ctx->kb_start_pan_y + (ctx->kb_end_pan_y - ctx->kb_start_pan_y) * smooth;
	}

	gs_effect_set_texture(p_image, tex);
	gs_effect_set_float(p_t, t);
	gs_effect_set_int(p_tt, static_cast<int>(ctx->transition_type));
	gs_effect_set_bool(p_fo, ctx->state == DSState::FadeOut);
	gs_effect_set_float(p_kb_zoom, kb_zoom_val);
	gs_effect_set_float(p_kb_pan_x, kb_pan_x_val);
	gs_effect_set_float(p_kb_pan_y, kb_pan_y_val);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	while (gs_effect_loop(eff, "Draw"))
		gs_draw_sprite(tex, 0, 0, 0);

	gs_blend_state_pop();
}

uint32_t driftslide_get_width(void *data)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);
	if (ctx->img_width > 0)
		return ctx->img_width;
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi))
		return ovi.base_width;
	return 1920;
}

uint32_t driftslide_get_height(void *data)
{
	auto *ctx = static_cast<DriftSlideSource *>(data);
	if (ctx->img_height > 0)
		return ctx->img_height;
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi))
		return ovi.base_height;
	return 1080;
}
