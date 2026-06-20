/*
DriftSlide – OBS image slideshow source plugin
Copyright (C) 2026 Stefan Pommerening

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "driftslide-source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static struct obs_source_info driftslide_info = {};

bool obs_module_load(void)
{
	driftslide_info.id           = "driftslide_source";
	driftslide_info.type         = OBS_SOURCE_TYPE_INPUT;
	driftslide_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB | OBS_SOURCE_DO_NOT_DUPLICATE;
	driftslide_info.get_name     = [](void *) -> const char * { return "DriftSlide"; };
	driftslide_info.create          = driftslide_create;
	driftslide_info.destroy         = driftslide_destroy;
	driftslide_info.update          = driftslide_update;
	driftslide_info.get_defaults    = driftslide_get_defaults;
	driftslide_info.get_properties  = driftslide_get_properties;
	driftslide_info.video_tick      = driftslide_video_tick;
	driftslide_info.video_render    = driftslide_video_render;
	driftslide_info.get_width       = driftslide_get_width;
	driftslide_info.get_height      = driftslide_get_height;
	driftslide_info.icon_type       = OBS_ICON_TYPE_SLIDESHOW;
	obs_register_source(&driftslide_info);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
