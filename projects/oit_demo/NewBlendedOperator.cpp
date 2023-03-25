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

#include "OITDemoApplication.h"
#include "shaders/Common.hlsli"

void OITDemoApp::SetupNewBlendedOperator()
{
}

void OITDemoApp::RecordNewBlendedOperator()
{
    mCommandBuffer->TransitionImageLayout(
        mTransparencyPass,
        grfx::RESOURCE_STATE_SHADER_RESOURCE, grfx::RESOURCE_STATE_RENDER_TARGET,
        grfx::RESOURCE_STATE_SHADER_RESOURCE, grfx::RESOURCE_STATE_DEPTH_STENCIL_WRITE);
    mCommandBuffer->BeginRenderPass(mTransparencyPass, grfx::DRAW_PASS_CLEAR_FLAG_CLEAR_RENDER_TARGETS);

    mCommandBuffer->SetScissors(mTransparencyPass->GetScissor());
    mCommandBuffer->SetViewports(mTransparencyPass->GetViewport());

    mCommandBuffer->EndRenderPass();
    mCommandBuffer->TransitionImageLayout(
        mTransparencyPass,
        grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_SHADER_RESOURCE,
        grfx::RESOURCE_STATE_DEPTH_STENCIL_WRITE, grfx::RESOURCE_STATE_SHADER_RESOURCE);
}

