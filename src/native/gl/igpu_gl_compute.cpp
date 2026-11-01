#include "igpu_gl_compute.h"
#include "igpu_gl_uniform.h"
#include "igpu_gl_graphics.h"
#include "igpu_gl_loader.h"
#include "igpu_error.h"
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace igpu
{
    namespace
    {
        constexpr GLenum ssbo = 0x90D2, ssbo_binding = 0x90D3;
        constexpr GLenum image_fields[] = {0x8F3A,0x8F3B,0x8F3C,0x8F3D,0x8F3E,0x906E};
        bool context_compute = false;
        GLint group_limits[3]{}, binding_count = 0, block_limit = 0, image_count = 0, compute_images = 0;
        GLint stage_blocks[6]{};
        bool fail(const char* entry, const char* message)
        { set_last_error(std::string(entry) + ": " + message); return false; }
        struct GenericBinding
        {
            GLint saved = 0;
            GenericBinding() { glGetIntegerv(ssbo_binding, &saved); }
            ~GenericBinding() { igpu_gl_fns().BindBuffer(ssbo, static_cast<GLuint>(saved)); }
        };
        struct BufferBinding
        {
            GLint name = 0;
            std::int64_t offset = 0, size = 0;
            void capture(GLuint slot)
            {
                auto& f = igpu_gl_fns();
                f.GetIntegeri(ssbo_binding, slot, &name);
                f.GetInteger64i(0x90D4 /* START */, slot, &offset);
                f.GetInteger64i(0x90D5 /* SIZE */, slot, &size);
            }
            void restore(GLuint slot) const
            {
                auto& f = igpu_gl_fns();
                if (name && size > 0)
                    f.BindBufferRange(ssbo, slot, static_cast<GLuint>(name), static_cast<std::ptrdiff_t>(offset), static_cast<std::ptrdiff_t>(size));
                else f.BindBufferBase(ssbo, slot, static_cast<GLuint>(name));
            }
        };
        struct ImageBinding
        {
            std::array<GLint,6> values{};
            void capture(GLuint slot)
            { for (int i = 0; i < 6; ++i) igpu_gl_fns().GetIntegeri(image_fields[i], slot, &values[i]); }
            void restore(GLuint slot) const
            {
                igpu_gl_fns().BindImageTexture(slot, static_cast<GLuint>(values[0]), values[1],
                    static_cast<GLboolean>(values[2]), values[3], static_cast<GLenum>(values[4]), static_cast<GLenum>(values[5]));
            }
        };
        struct Claim { std::uint64_t buffer = 0; BufferBinding previous; };
        std::unordered_map<int,Claim> claims; // key = stage*8+slot, physical binding = 8+key
        bool read_i64(const gm::wire::GMValue& value, std::int64_t& out)
        {
            if (value.is<std::int32_t>()) { out = value.as<std::int32_t>(); return true; }
            if (value.is<std::uint64_t>())
            {
                const auto raw = value.as<std::uint64_t>();
                if (raw > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) return false;
                out = static_cast<std::int64_t>(raw); return true;
            }
            if (value.is<double>())
            {
                const auto raw = value.as<double>();
                if (!std::isfinite(raw) || raw < -9223372036854775808.0 || raw >= 9223372036854775808.0 || std::trunc(raw) != raw) return false;
                out = static_cast<std::int64_t>(raw); return true;
            }
            return false;
        }
        struct Write
        {
            int kind = 0, mip = 0;
            std::uint64_t handle = 0;
            GlStorageBuffer buffer;
            GlStorageTexture texture;
        };
        struct DispatchState
        {
            GLint program = 0;
            GenericBinding generic;
            std::vector<Write>& writes;
            std::array<BufferBinding,8> buffers;
            std::array<ImageBinding,8> images;
            explicit DispatchState(std::vector<Write>& targets) : writes(targets)
            {
                glGetIntegerv(0x8B8D /* CURRENT_PROGRAM */, &program);
                for (GLuint i = 0; i < writes.size(); ++i)
                    if (writes[i].kind == 0) images[i].capture(i); else buffers[i].capture(i);
            }
            ~DispatchState()
            {
                for (GLuint i = 0; i < writes.size(); ++i)
                    if (writes[i].kind == 0) images[i].restore(i); else buffers[i].restore(i);
                igpu_gl_fns().UseProgram(static_cast<GLuint>(program));
            }
        };
        bool execute(GLuint program, int x, int y, int z, std::vector<Write>& writes, const char* entry)
        {
            if (!gl_compute_supported()) return fail(entry, "this OpenGL context does not support compute storage writes");
            const int groups[] = {x,y,z};
            for (int axis = 0; axis < 3; ++axis)
                if (groups[axis] <= 0 || groups[axis] > 65535 || groups[axis] > group_limits[axis])
                    return fail(entry, "group counts must fit the context limits and 1..65535");
            if (!program) return fail(entry, "bind a compute shader first");
            if (writes.empty() || writes.size() > 8) return fail(entry, "pass 1 to 8 write targets");
            GLboolean feedback_active = GL_FALSE, feedback_paused = GL_FALSE;
            glGetBooleanv(0x8E24 /* TRANSFORM_FEEDBACK_ACTIVE */, &feedback_active);
            if (feedback_active) glGetBooleanv(0x8E23 /* TRANSFORM_FEEDBACK_PAUSED */, &feedback_paused);
            if (feedback_active && !feedback_paused)
                return fail(entry, "pause or end transform feedback before dispatch");
            GLint current = 0, deleted = 0;
            glGetIntegerv(0x8B8D, &current);
            if (current) igpu_gl_fns().GetProgramiv(current, 0x8B80 /* DELETE_STATUS */, &deleted);
            if (deleted) return fail(entry, "unbind the current program pending deletion before dispatch");
            if (!gl_uniform_validate_inputs(2, 2, entry)) return false;
            // A caller can map an input after claiming it; validate again at dispatch.
            for (const auto& claim : claims)
            {
                if (claim.first / 8 != 2) continue;
                GlStorageBuffer input;
                if (!gl_storage_buffer(claim.second.buffer, input, entry)) return false;
            }
            for (std::size_t i = 0; i < writes.size(); ++i)
            {
                auto& write = writes[i];
                if (!write.handle) return fail(entry, "a target entry is not a handle");
                for (std::size_t previous = 0; previous < i; ++previous)
                    if (write.kind == writes[previous].kind && write.handle == writes[previous].handle)
                        return fail(entry, "the same write target is listed twice");
                if (write.kind == 0)
                {
                    if (!gl_storage_texture(write.handle, write.mip, write.texture, entry)) return false;
                }
                else if (write.kind == 1)
                {
                    if (!gl_storage_buffer(write.handle, write.buffer, entry)) return false;
                    if (write.buffer.bytes > block_limit) return fail(entry, "the storage buffer exceeds the context block size limit");
                    for (const auto& claim : claims)
                        if (claim.first / 8 == 2 && claim.second.buffer == write.handle)
                            return fail(entry, "a compute storage input cannot also be a write target");
                }
                else return fail(entry, "unknown write target kind");
            }
            auto& f = igpu_gl_fns();
            DispatchState restore(writes);
            f.UseProgram(program);
            for (GLuint i = 0; i < writes.size(); ++i)
            {
                const auto& write = writes[i];
                if (write.kind == 0) f.BindImageTexture(i, write.texture.name, write.texture.level, write.texture.layered ? GL_TRUE : GL_FALSE, 0,
                                                      0x88BA /* READ_WRITE */, write.texture.format);
                else f.BindBufferBase(ssbo, i, write.buffer.name);
            }
            f.DispatchCompute(static_cast<GLuint>(x), static_cast<GLuint>(y), static_cast<GLuint>(z));
            // Writes are visible to CPU readback, texture fetches and later GPU commands.
            f.MemoryBarrier(0xFFFFFFFFu /* ALL_BARRIER_BITS */);
            return true;
        }
    }

    void gl_compute_initialize()
    {
        // Program switching must be able to inspect active/paused transform feedback.
        const bool feedback_queries = gl_context_at_least(4,0) || gl_context_extension("GL_ARB_transform_feedback2");
        context_compute = feedback_queries && (gl_context_at_least(4,3) ||
            (gl_context_extension("GL_ARB_compute_shader") && gl_context_extension("GL_ARB_shader_storage_buffer_object") &&
             gl_context_extension("GL_ARB_shader_image_load_store")));
        binding_count = block_limit = image_count = compute_images = 0;
        for (auto& value : group_limits) value = 0;
        for (auto& value : stage_blocks) value = 0;
        if (!context_compute || !igpu_gl_fns().GetIntegeri) return;
        auto& f = igpu_gl_fns();
        for (int i = 0; i < 3; ++i)
            f.GetIntegeri(0x91BE /* MAX_COMPUTE_WORK_GROUP_COUNT */, i, &group_limits[i]);
        const bool geometry = gl_context_at_least(3, 2);
        const bool tessellation = gl_context_at_least(4, 0) || gl_context_extension("GL_ARB_tessellation_shader");
        const GLenum block_fields[] = {0x90D6,0x90DA,0x90DB,0x90D7,0x90D8,0x90D9};
        const bool stages[] = {true,true,true,geometry,tessellation,tessellation};
        for (int i = 0; i < 6; ++i)
            if (stages[i]) glGetIntegerv(block_fields[i], &stage_blocks[i]);
        glGetIntegerv(0x90DD /* MAX_SHADER_STORAGE_BUFFER_BINDINGS */, &binding_count);
        glGetIntegerv(0x90DE /* MAX_SHADER_STORAGE_BLOCK_SIZE */, &block_limit);
        glGetIntegerv(0x8F38 /* MAX_IMAGE_UNITS */, &image_count);
        glGetIntegerv(0x91BD /* MAX_COMPUTE_IMAGE_UNIFORMS */, &compute_images);
    }
    bool gl_compute_supported()
    {
        const auto& f = igpu_gl_fns();
        return context_compute && f.DispatchCompute && f.MemoryBarrier && f.BindImageTexture &&
            f.BindBufferBase && f.BindBufferRange && f.GetIntegeri && f.GetInteger64i &&
            binding_count >= 8 && stage_blocks[2] >= 8 && block_limit > 0 && image_count >= 8 && compute_images >= 8 &&
            group_limits[0] > 0 && group_limits[1] > 0 && group_limits[2] > 0;
    }
    bool gl_read_storage_supported()
    { return gl_compute_supported() && binding_count >= 32 && stage_blocks[0] >= 8 && stage_blocks[1] >= 8; }

    bool gl_storage_bind(std::uint64_t buffer, std::int32_t stage, std::int32_t slot)
    {
        const char* entry = "igpu_storage_bind";
        if (!gl_read_storage_supported()) return fail(entry, "this OpenGL context does not support storage input bindings");
        if (stage < 0 || stage > 5 || slot < 0 || slot >= 8) return fail(entry, "use stages0..5 and slots0..7");
        if (stage >= 3 && (!gl_graphics_stage_supported(stage) || stage_blocks[stage] < 8 || 8 + stage * 8 + slot >= binding_count))
            return fail(entry, "the requested graphics stage/slot exceeds this context's storage input support");
        const int key = stage * 8 + slot;
        const auto existing = claims.find(key);
        if (!buffer)
        {
            if (existing != claims.end())
            {
                GenericBinding generic;
                existing->second.previous.restore(static_cast<GLuint>(8 + key));
                claims.erase(existing);
            }
            return true;
        }
        GlStorageBuffer resolved;
        if (!gl_storage_buffer(buffer, resolved, entry)) return false;
        if (resolved.bytes > block_limit) return fail(entry, "the storage buffer exceeds the context block size limit");
        GenericBinding generic;
        if (existing == claims.end())
        {
            Claim claimed;
            claimed.buffer = buffer;
            claimed.previous.capture(static_cast<GLuint>(8 + key));
            claims.emplace(key, claimed);
        }
        else existing->second.buffer = buffer;
        igpu_gl_fns().BindBufferBase(ssbo, static_cast<GLuint>(8 + key), resolved.name);
        return true;
    }
    bool gl_storage_validate_inputs(int first_stage, int last_stage, const char* entry)
    {
        for (const auto& claim : claims)
        {
            const int stage = claim.first / 8;
            if (stage < first_stage || stage > last_stage) continue;
            GlStorageBuffer input;
            if (!gl_storage_buffer(claim.second.buffer, input, entry)) return false;
            if (input.bytes > block_limit) return fail(entry, "the storage input exceeds the context block size limit");
        }
        return true;
    }
    void gl_storage_release_buffer(std::uint64_t buffer)
    {
        if (claims.empty()) return;
        GenericBinding generic;
        for (auto it = claims.begin(); it != claims.end();)
        {
            if (it->second.buffer != buffer) { ++it; continue; }
            it->second.previous.restore(static_cast<GLuint>(8 + it->first));
            it = claims.erase(it);
        }
    }
    void gl_compute_resources_release()
    {
        if (claims.empty()) return;
        GenericBinding generic;
        for (const auto& claim : claims) claim.second.previous.restore(static_cast<GLuint>(8 + claim.first));
        claims.clear();
    }
    bool gl_dispatch_storage(std::uint32_t program, std::int32_t x, std::int32_t y, std::int32_t z,
                             const gm::wire::GMArrayView& kinds, const gm::wire::GMArrayView& targets)
    {
        const char* entry = "igpu_dispatch_writes";
        if (kinds.size() != targets.size() || kinds.size() < 1 || kinds.size() > 8)
            return fail(entry, "pass 1 to 8 entries with equal array lengths");
        std::vector<Write> writes;
        for (std::size_t i = 0; i < kinds.size(); ++i)
        {
            std::int64_t kind = 0, handle = 0;
            if (!read_i64(kinds[i], kind) || (kind != 0 && kind != 1) || !read_i64(targets[i], handle) || handle <= 0)
                return fail(entry, "a kind or target entry is invalid");
            Write write;
            write.kind = static_cast<int>(kind); write.handle = static_cast<std::uint64_t>(handle);
            writes.push_back(write);
        }
        return execute(program,x,y,z,writes,entry);
    }
    bool gl_dispatch_single(std::uint32_t program, std::int32_t x, std::int32_t y, std::int32_t z,
                            std::uint64_t texture, std::int32_t mip, std::uint64_t buffer)
    {
        std::vector<Write> writes;
        if (texture) { Write write; write.handle = texture; write.mip = mip; writes.push_back(write); }
        if (buffer) { Write write; write.kind = 1; write.handle = buffer; writes.push_back(write); }
        return execute(program,x,y,z,writes,"igpu_dispatch");
    }
}
