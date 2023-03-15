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

struct RenderParameters
{
    float4x4 backgroundMVP;
    float4x4 meshMVP;
    float meshOpacity;
};

#if defined(IS_SHADER)

// ConstantBuffer was added in SM5.1 for D3D12
#if defined(PPX_D3D11)
cbuffer Parameters : register(b0)
{
    RenderParameters Parameters;
};
#else
ConstantBuffer<RenderParameters> Parameters : register(b0);
#endif

#endif

