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

////////////////////////////////////////////////////////////////////////////////

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput vsmain(uint VertexID : SV_VertexID)
{
    const float4 vertices[] =
    {
        float4(-1.0f, -1.0f, +0.0f, +1.0f),
        float4(+3.0f, -1.0f, +2.0f, +1.0f),
        float4(-1.0f, +3.0f, +0.0f, -1.0f),
    };

    VSOutput result;
    result.Position = float4(vertices[VertexID].xy, 0.0f, 1.0f);
    result.TexCoord = float2(vertices[VertexID].zw);
    return result;
}

////////////////////////////////////////////////////////////////////////////////

SamplerState CompositeSampler    : register(COMPOSITE_SAMPLER_REGISTER);
Texture2D    OpaqueTexture       : register(OPAQUE_TEXTURE_REGISTER);
Texture2D    TransparencyTexture : register(TRANSPARENCY_TEXTURE_REGISTER);

float4 psmain(VSOutput input) : SV_TARGET
{
	const float3 opaqueColor = OpaqueTexture.Sample(CompositeSampler, input.TexCoord).rgb;

	const float4 transparencySample = TransparencyTexture.Sample(CompositeSampler, input.TexCoord);
	const float3 transparencyColor = transparencySample.rgb;
	const float coverage = transparencySample.a;

	const float3 finalColor = transparencyColor + ((1.0f - coverage) * opaqueColor);
	return float4(finalColor, 1.0f);
}

