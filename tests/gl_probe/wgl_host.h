#pragma once

#include <string>

// Creates a hidden window and an unversioned WGL context, then makes that
// context current. The caller owns the context: IGPU must not create or
// delete it. gl_probe_end destroys whatever this process still holds.
bool gl_probe_begin(std::string& error);
void gl_probe_end();
void* gl_probe_context();
