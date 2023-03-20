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

#include "Common.hlsli"

#if defined(PPX_D3D11)
cbuffer Globals : register(SHADER_GLOBALS_REGISTER)
{
    ShaderGlobals g_Globals;
};
StructuredBuffer<Pixel> g_Pixels : register(GRAPHICS_BUFFER_REGISTER);
#else
ConstantBuffer<ShaderGlobals> g_Globals : register(SHADER_GLOBALS_REGISTER, space0);
StructuredBuffer<Pixel> g_Pixels : register(GRAPHICS_BUFFER_REGISTER, space0);
#endif

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 Color    : COLOR;
};

VSOutput vsmain(uint VertexID : SV_VertexID)
{
	const float2 vertices[] =
	{
		float2(-1.0f, -1.0f),
		float2(+1.0f, -1.0f),
		float2(-1.0f, +1.0f),

		float2(-1.0f, +1.0f),
		float2(+1.0f, -1.0f),
		float2(+1.0f, +1.0f),
	};

	const float2 scale = float2(
		g_Globals.Resolution.z / g_Globals.Resolution.x,
		g_Globals.Resolution.w / g_Globals.Resolution.y);

	VSOutput result;
	result.Position = float4(vertices[VertexID] * scale, 0.0f, 1.0f);
	result.Color = float3(1.0f, 0.0f, 0.0f);
	return result;
}

float4 psmain(VSOutput input) : SV_TARGET
{
	return float4(input.Color, 1.0f);
}

