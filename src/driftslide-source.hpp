#pragma once

#include <obs-module.h>
#include <graphics/image-file.h>

#include <memory>
#include <mutex>
#include <string>

#include "image-list.hpp"

enum class DSState { Transparent, FadeIn, Displaying, FadeOut };

enum class TransitionType {
	Fade = 0,
	SlideLeft,
	SlideRight,
	SlideUp,
	SlideDown,
};

struct DriftSlideSource {
	obs_source_t *source = nullptr;

	// Pending settings – written by UI thread under settings_mutex
	std::mutex settings_mutex;
	bool settings_dirty = false;
	std::string pending_dir;
	bool pending_random = false;
	float pending_tdur = 30.0f;
	float pending_ddur = 15.0f;
	float pending_trdur = 2.0f;
	TransitionType pending_ttype = TransitionType::Fade;

	// Active settings – render/tick thread only (no lock needed after copy)
	std::string directory;
	bool random_order = false;
	float transparent_dur = 30.0f;
	float display_dur = 15.0f;
	float transition_dur = 2.0f;
	TransitionType transition_type = TransitionType::Fade;

	// State machine – render/tick thread only
	DSState state = DSState::Transparent;
	float state_timer = 0.0f;

	// Image list – render/tick thread only
	std::unique_ptr<ImageList> image_list;

	// Cached texture dimensions
	uint32_t img_width = 0;
	uint32_t img_height = 0;

	// Graphics resources
	gs_image_file4_t if4 = {};
	bool texture_loaded = false;
	gs_effect_t *slide_effect = nullptr;
};

void *driftslide_create(obs_data_t *settings, obs_source_t *source);
void driftslide_destroy(void *data);
void driftslide_update(void *data, obs_data_t *settings);
void driftslide_get_defaults(obs_data_t *settings);
obs_properties_t *driftslide_get_properties(void *data);
void driftslide_video_tick(void *data, float seconds);
void driftslide_video_render(void *data, gs_effect_t *effect);
uint32_t driftslide_get_width(void *data);
uint32_t driftslide_get_height(void *data);
