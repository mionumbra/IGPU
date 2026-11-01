# OpenGL native draw diagnostics

The existing C++ diagnostics facade in native/igpu_draw.h now reports GL drawing and actual vertex input state. These native compatibility diagnostics are not new slim GameMaker exports. API/version remains 0.5.0, generated resources are unchanged, and the formal Windows DLL remains D3D11.

## Counters

igpu::draw_count() increases once when an implemented public backend submission returns accepted. This includes ordinary, indexed, instanced, array/indexed indirect, patch, MRT and sampled drawing. An indirect command that requests no GPU work is still one accepted submission. Nested target helpers and private comparison-copy draws do not add submissions. Validation failures, queries and compute dispatch do not increase this counter.

igpu::draw_restore_failures() compares actual native vertex input state before and after each public attempt, including attempts that return false. Any mismatch adds one failure, even if multiple fields differ. A submitted draw retains its result; the diagnostic records a restoration problem rather than pretending the submitted work can be rolled back.

The snapshot covers the caller VAO, generic ARRAY_BUFFER, that VAO's ELEMENT_ARRAY_BUFFER, supported DRAW_INDIRECT_BUFFER, and attribute 0/1 buffer, enable, size, type, stride, normalization and pointer state. Integer and divisor attributes use their actual context/extension gates. LONG attribute queries require GL4.3; GL4.1 or ARB_vertex_attrib_64bit format support alone does not guarantee that query token. A core-profile VAO0 is no object, so its absent element/attribute state is skipped. Compatibility default VAO0 remains a valid object and is inspected normally. The count describes input-state restoration, matching the existing D3D diagnostic scope; render-target and raster/depth state retain their existing draw contracts.

Counters saturate at the maximum signed 32-bit value and retain GL process totals across backend shutdown/rebind, like the existing D3D counters. With no active backend, the common facade returns zero. Diagnostics do not consume pending GL errors, mutate native state or overwrite the submission's last-error text.

## Actual vertex source

igpu::is_vertex_buffer_bound(handle) resolves an existing Vertex buffer and reads the current VAO's attribute 0 buffer source. A different generic ARRAY_BUFFER binding does not change that source; conversely, a matching generic binding alone does not make a buffer a vertex source. A disabled attribute still retains its native source association. With core-profile VAO0, the query returns false without asking for nonexistent attribute state. Unknown, zero, released and wrong-role handles return false quietly through the existing facade.

Buffer handles remain monotonic across GL shutdown, so a stale buffer handle cannot alias a fresh vertex buffer. Shutdown deletes the owned private draw VAO before discarding its name and owned buffers. Foreign caller VAOs, buffers, bindings, bytes and the borrowed WGL context are preserved.

## Verification

Observed REDs:
- out/diagnostics-red-run.log: native attribute0 query reaches the old not-implemented stub.
- out/diagnostics-lifetime-red-run.log: the owned draw VAO remains a live native object after shutdown.
- out/diagnostics-identity-red-run.log: a fresh buffer reuses the stale pre-shutdown handle.

The complete WGL evidence is out/diagnostics-complete-build.log and out/diagnostics-complete-run.log. Tests verify real sixteen-sample pixels across all drawing families (thirty-two samples for two instances), sampled/patch/MRT count-once behavior, comparison-helper exclusion, an actual compute word123 without draw increments, quiet native binding queries, invalid submissions and all prior suites.

Fault injection deliberately drops a real VAO restore, changes a divisor or attribute format/pointer while bindings remain equal, and drops an indirect-buffer restore. Each adds exactly one observed restoration failure; subsequent clean drawing does not. A preexisting GL_INVALID_ENUM survives a valid draw and its diagnostic queries. Shutdown tests inspect native owned-VAO deletion, foreign VAO/buffer lifetime and bytes, inactive facade zero, cumulative rebind totals and fresh drawing with non-aliased handles. DLL evidence: out/diagnostics-dll-build.log.

Independent review added out/diagnostics-review-red-run.log and out/diagnostics-review-green-run.log: a real GL4.1 core context verifies VAO0 binding, accepted/rejected submissions and restoration without added GLerror. A strict4.1 driver shim rejects any attempted LONG token, proving the4.3 gate, while a compatibility defaultVAO0 positive-source test preserves the older contract. The original borrowed context is rebound before final probe shutdown.
