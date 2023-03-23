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
	float2 PixelCoord : TEXCOORD;
};

VSOutput vsmain(uint VertexID : SV_VertexID)
{
	const float windowW = g_Globals.Resolution.x;
	const float windowH = g_Globals.Resolution.y;
	const float screenW = g_Globals.Resolution.z;
	const float screenH = g_Globals.Resolution.w;

	const float4 vertices[] =
	{
		float4(-1.0f, -1.0f, 0.0f,    screenH),
		float4(+1.0f, -1.0f, screenW, screenH),
		float4(-1.0f, +1.0f, 0.0f,    0.0f),

		float4(-1.0f, +1.0f, 0.0f,    0.0f),
		float4(+1.0f, -1.0f, screenW, screenH),
		float4(+1.0f, +1.0f, screenW, 0.0f),
	};

	const float2 scale = float2(screenW / windowW, screenH / windowH);

	VSOutput result;
	result.Position = float4(vertices[VertexID].xy * scale, 0.0f, 1.0f);
	result.PixelCoord = vertices[VertexID].zw;
	return result;
}

float4 psmain(VSOutput input) : SV_TARGET
{
	const uint pixelIndex = (uint)input.PixelCoord.y * GRAPHICS_BUFFER_WIDTH + (uint)input.PixelCoord.x;
	const float4 color = g_Pixels.Load(pixelIndex).Color;
	return float4(color.xyz, 1.0f);
}

