# GL draw diagnostics batch

Finish the existing native draw_count, draw_restore_failures and is_vertex_buffer_bound diagnostics facade on GL while preserving API0.5.0 and the formal D3D11DLL. Standing authorization covers continuous inline implementation, full verification, one whole-batch review, featurePR and exact-head merge.

Count one accepted public submission across ordinary/indexed/instanced/arrays-indirect/indexed-indirect/patch/MRT/sampled calls. Rejected validation, dispatch/queries and internal comparison-mirror passes do not count. Counters saturate at int32 maximum and persist for the process like the existing D3D counters; when no backend is active the shared facade returns0. Rebinding GL exposes its existing process totals.

Use a focused diagnostics module at the backend submission boundary, preserving private helper behavior. Save actual native vertex-input state before each public submission and compare after return, including callerVAO, array/element/indirect bindings, attribute0/1 formats/pointers/buffers/enables and supported divisors. Count one restoration failure per public attempt with any mismatch, independent of accepted drawcount; never claim success by returning a constant0. Diagnostics do not consume caller GL errors, mutate native state or replace the draw's diagnostic. Add required core2 GetVertexAttribiv/GetVertexAttribPointerv loader entries.

The binding query reads the currentVAO attribute0 source, even when the genericARRAY_BUFFER names another buffer. Disabledattribute0 still has an associated native source. Unknown/released/wrongrole handles returnfalse without raising an error or altering native state. Resolve handles in existing draw resource storage, not a duplicate registry.

Tests must observe the oldstubs fail. Verify actual16sample pixels and counterdelta1 on all submission families; failures unchanged; MRT and sampled mirrors countonce; exactstate preservation; real nativefault injection that drops an actual restore and increments thefailurecounter, then a clean draw doesnotincrement again; foreignVAO/nativebuffer distinction; noERROR leakage; shutdown/rebind lifetime. Complete historicalWGLprobe, DLL, guides/handover/diff checks, one independentreview and mergeapprovedhead.

Lifecycle also covers deleting the owned drawVAO (existing gl_resources_release discards its name without deletion) and keeping bufferhandles monotonic across GLshutdown so a stale vertexbuffer diagnostic cannot alias a newly created buffer. These are part of actual drawinput resource lifetime, with native IsVertexArray and freshbinding evidence.

The generated slim0.5 GML exports intentionally do not expose these legacy diagnostic wrappers. Verify through the existing native igpu_draw.h facade; this batch adds no GML export or generatedresource edit.
