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
#include "FullscreenVS.hlsli"

SamplerState NearestSampler : register(CUSTOM_SAMPLER_REGISTER);

Texture2D    LayerTexture0  : register(CUSTOM_TEXTURE_0_REGISTER);
Texture2D    LayerTexture1  : register(CUSTOM_TEXTURE_1_REGISTER);
Texture2D    LayerTexture2  : register(CUSTOM_TEXTURE_2_REGISTER);
Texture2D    LayerTexture3  : register(CUSTOM_TEXTURE_3_REGISTER);
Texture2D    LayerTexture4  : register(CUSTOM_TEXTURE_4_REGISTER);
Texture2D    LayerTexture5  : register(CUSTOM_TEXTURE_5_REGISTER);
Texture2D    LayerTexture6  : register(CUSTOM_TEXTURE_6_REGISTER);
Texture2D    LayerTexture7  : register(CUSTOM_TEXTURE_7_REGISTER);

void MergeColor(inout float4 outColor, float4 layerColor)
{
    outColor.rgb = lerp(outColor.rgb, layerColor.rgb, layerColor.a);
    outColor.a *= 1.0f - layerColor.a;
}

float4 psmain(VSOutput input) : SV_TARGET
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    MergeColor(color, LayerTexture7.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture6.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture5.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture4.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture3.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture2.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture1.Sample(NearestSampler, input.uv));
    MergeColor(color, LayerTexture0.Sample(NearestSampler, input.uv));

    color.a = 1.0f - color.a;
    return color;
}
