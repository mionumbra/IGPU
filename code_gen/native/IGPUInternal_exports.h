// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#include "core/GMExtUtils.h"

// Internal function used for queueing buffers to native code
GMEXPORT double __EXT_NATIVE__IGPU_queue_buffer(char* __arg_buffer, double __arg_buffer_length);

GMEXPORT double __EXT_NATIVE__igpu_init(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_bind_current();
GMEXPORT double __EXT_NATIVE__igpu_shutdown();
GMEXPORT char* __EXT_NATIVE__igpu_version();
GMEXPORT double __EXT_NATIVE__igpu_is_available();
GMEXPORT double __EXT_NATIVE__igpu_device_lost();
GMEXPORT double __EXT_NATIVE__igpu_get_feature_level();
GMEXPORT char* __EXT_NATIVE__igpu_get_adapter_description();
GMEXPORT double __EXT_NATIVE__igpu_get_video_memory(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_width();
GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_height();
GMEXPORT double __EXT_NATIVE__igpu_get_capabilities(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_supports(double capability);
GMEXPORT char* __EXT_NATIVE__igpu_get_shader_dialect();
GMEXPORT double __EXT_NATIVE__igpu_set_graphics_info(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_compile(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_bind(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_get_bound_shader(double stage, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT char* __EXT_NATIVE__igpu_get_last_error();
GMEXPORT double __EXT_NATIVE__igpu_input_layout_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_input_layout_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_write(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_resize(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_read(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_size(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_storage_bind(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_reflect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_uniform_write(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_uniform_bind(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_indexed(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_indirect(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_patch(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_indexed_indirect(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_blend_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_depth_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_raster_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_sampler_state_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_state_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_texture_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_texture_generate_mips(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_texture_read(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_texture_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_dispatch(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_create(double kind, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_begin(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_end(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_ready(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_result(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_query_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_timestamp_frequency(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_fence_create(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_fence_signal(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_fence_signaled(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_fence_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_to_render_targets(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_sampled(char* __arg_buffer, double __arg_buffer_length);

