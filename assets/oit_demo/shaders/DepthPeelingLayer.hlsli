// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define IS_SHADER
#include "Common.hlsli"
#include "TransparencyVS.hlsli"

SamplerComparisonState ComparisonSampler_Greater : register(CUSTOM_SAMPLER_0_REGISTER);
SamplerComparisonState ComparisonSampler_Less    : register(CUSTOM_SAMPLER_1_REGISTER);
Texture2D              OpaqueDepthTexture        : register(CUSTOM_TEXTURE_0_REGISTER);
#if !defined(IS_FIRST_LAYER)
Texture2D              LayerDepthTexture         : register(CUSTOM_TEXTURE_1_REGISTER);
#endif

float4 psmain(VSOutput input) : SV_TARGET
{
    float2 textureDimension = (float2)0;
    OpaqueDepthTexture.GetDimensions(textureDimension.x, textureDimension.y);
    // Apply opaque depth
    {
        const float compareResult = OpaqueDepthTexture.SampleCmpLevelZero(ComparisonSampler_Less, input.position.xy / textureDimension, input.position.z);
        clip(compareResult - 0.5f);
    }
#if !defined(IS_FIRST_LAYER)
    // Apply front layer depth
    {
        const float compareResult = LayerDepthTexture.SampleCmpLevelZero(ComparisonSampler_Greater, input.position.xy / textureDimension, input.position.z);
        clip(compareResult - 0.5f);
    }
#endif
    return float4(input.color, g_Globals.meshOpacity);
}
