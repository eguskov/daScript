#pragma once

#include "daScript/simulate/runtime_matrices.h"

namespace das {
    void float4x4_identity ( float4x4 & mat );
    void float3x4_identity ( float3x4 & mat );
    float4x4 float_4x4_translation(float3 xyz);
    float3x4 float3x4_mul(const float3x4 &a, const float3x4 &b);
}

