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

RWTexture2D<uint>                        CountTexture   : register(CUSTOM_UAV_0_REGISTER);
RWStructuredBuffer<BufferBucketFragment> FragmentBuffer : register(CUSTOM_UAV_1_REGISTER);

void MergeColor(inout float4 outColor, float4 fragmentColor)
{
    outColor.rgb = lerp(outColor.rgb, fragmentColor.rgb, fragmentColor.a);
    outColor.a *= 1.0f - fragmentColor.a;
}

float4 psmain(VSOutput input) : SV_TARGET
{
    const uint2 bucketIndex  = (uint2)input.position.xy;
    const uint fragmentCount = min(CountTexture[bucketIndex], BUFFER_BUCKET_SIZE_PER_PIXEL);
    CountTexture[bucketIndex] = 0U; // Reset fragment count for the next frame

    BufferBucketFragment sortedEntries[BUFFER_BUCKET_SIZE_PER_PIXEL];

    // Copy the fragments into local memory for sorting
    {
        float2 countTextureDimension = (float2)0;
        CountTexture.GetDimensions(countTextureDimension.x, countTextureDimension.y);
        const uint fragmentBaseIndex = ((bucketIndex.y * (uint)countTextureDimension.x) + bucketIndex.x) * BUFFER_BUCKET_SIZE_PER_PIXEL;
        for(uint i = 0; i < fragmentCount; ++i)
        {
            sortedEntries[i] = FragmentBuffer[fragmentBaseIndex + i];
        }
    }

    // Sort the fragments by depth (back to front)
    {
        for(uint i = 0; i < fragmentCount - 1; ++i)
        {
            for(uint j = i + 1; j < fragmentCount; ++j)
            {
                if(sortedEntries[j].depth > sortedEntries[i].depth)
                {
                    const BufferBucketFragment tmp = sortedEntries[i];
                    sortedEntries[i]               = sortedEntries[j];
                    sortedEntries[j]               = tmp;
                }
            }
        }
    }

    // Merge the fragments to get the final color
    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    for(uint i = 0; i < fragmentCount; ++i)
    {
        const BufferBucketFragment fragment = sortedEntries[i];
        MergeColor(color, fragment.color);
    }
    color.a = 1.0f - color.a;
    return color;
}
