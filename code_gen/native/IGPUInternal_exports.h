// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#include "core/GMExtUtils.h"

// Internal function used for queueing buffers to native code
GMEXPORT double __EXT_NATIVE__IGPU_queue_buffer(char* __arg_buffer, double __arg_buffer_length);

GMEXPORT double __EXT_NATIVE__igpu_init(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shutdown();
GMEXPORT char* __EXT_NATIVE__igpu_version();
GMEXPORT double __EXT_NATIVE__igpu_is_available();
GMEXPORT double __EXT_NATIVE__igpu_get_feature_level();
GMEXPORT char* __EXT_NATIVE__igpu_get_adapter_description();
GMEXPORT double __EXT_NATIVE__igpu_get_video_memory(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_width();
GMEXPORT double __EXT_NATIVE__igpu_get_backbuffer_height();
GMEXPORT double __EXT_NATIVE__igpu_get_capabilities(char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_supports(double capability);
GMEXPORT char* __EXT_NATIVE__igpu_get_shader_dialect();
GMEXPORT double __EXT_NATIVE__igpu_shader_compile(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_compile_vertex(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_compile_pixel(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_compile_compute(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_shader_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT char* __EXT_NATIVE__igpu_get_last_error();
GMEXPORT double __EXT_NATIVE__igpu_input_layout_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_input_layout_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_create(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_write(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_resize(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_read(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_size(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_buffer_release(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_draw_indexed(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__igpu_get_draw_count();
GMEXPORT double __EXT_NATIVE__igpu_get_draw_restore_failures();
GMEXPORT double __EXT_NATIVE__igpu_is_vertex_buffer_bound(char* __arg_buffer, double __arg_buffer_length);

