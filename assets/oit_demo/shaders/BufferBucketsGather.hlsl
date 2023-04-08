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

SamplerState                          NearestSampler     : register(CUSTOM_SAMPLER_0_REGISTER);
Texture2D                             OpaqueDepthTexture : register(CUSTOM_TEXTURE_0_REGISTER);
RWTexture2D<uint>                     CountTexture       : register(CUSTOM_UAV_0_REGISTER);
RWStructuredBuffer<BufferBucketEntry> EntryBuffer        : register(CUSTOM_UAV_1_REGISTER);

void psmain(VSOutput input)
{
    float2 textureDimension = (float2)0;
    CountTexture.GetDimensions(textureDimension.x, textureDimension.y);

    // Test against opaque depth
    {
        const float2 uv = input.position.xy / textureDimension;
        const float opaqueDepth = OpaqueDepthTexture.Sample(NearestSampler, uv).r;
        clip(input.position.z < opaqueDepth ? 1.0f : -1.0f);
    }

    // Find the next entry index in the pixel bucket
    const uint2 bucketIndex = (uint2)input.position.xy;
    uint nextBucketEntryIndex = 0;
    InterlockedAdd(CountTexture[bucketIndex], 1U, nextBucketEntryIndex);

    // Ignore the fragment if the bucket is already full
    if(nextBucketEntryIndex >= BUFFER_BUCKET_SIZE_PER_PIXEL)
    {
        clip(-1.0f);
    }

    // Add the fragment to the bucket
    const uint entryIndex = (((bucketIndex.y * (uint)textureDimension.x) + bucketIndex.x) * BUFFER_BUCKET_SIZE_PER_PIXEL) + nextBucketEntryIndex;
    EntryBuffer[entryIndex].color = float4(input.color, g_Globals.meshOpacity);
    EntryBuffer[entryIndex].depth = input.position.z;
}
