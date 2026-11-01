# OpenGL sampler axes and border color

The existing full sampler descriptor and native border-aware convenience constructors accept independent Clamp0, Repeat1, Mirror2 or Border3 modes on U/V/W. A supplied color is applied when any axis uses Border, including V-only and W-only cases; every draw restores the source texture's previous modes and border color. The implementation uses native S/T/R addressing and filtering.

Constructors without a border color retain their Clamp/Repeat/Mirror restriction. Invalid values outside0..3 are rejected before allocating a sampler. Comparison samplers still reject Border. Linear/mip filtering, bias/limits and supported hardware anisotropy keep their existing behavior and numeric validation. A supplied but unused color is valid on the native border-aware filter constructor.

For2D textures W does not participate in sampling; adding Border on W does not change U/V coordinates. A3D texture uses W for its depth direction, so W-only Border returns the supplied color for samples sufficiently beyond either end and blends the border at linear edges. Array layer selection and cube sampling retain native semantics. This batch adds no new sampler type, export or capability key.

Real-WGL coverage reuses existing fixtures: public full/native axes/native filters paths; U/V-only borders combined with clamp/repeat/mirror;2D W behavior; literal interior/exterior pixels; linear border interpolation; selected mip/range; hardware anisotropic V-only border when supported; real3D W positive/negative exterior/interior and linear edge with16 samples. Source S/T/R/border/filter/LOD and caller state restoration remain covered. Every prior GL and real4.1core suite is retained.

Actual failing evidence is out/mixed-border-red-run.log (3D) and out/mixed-border-2d-red-run.log (independent2D). Final combined results, formal D3D11 DLL and handover checks are recorded in the batch handover. API/version remains0.5.0, with no generated-resource or yy/yyp edits.
