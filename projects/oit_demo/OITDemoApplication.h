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

#include "ppx/ppx.h"
using namespace ppx;

class OITDemoApp
    : public ppx::Application
{
public:
    virtual void Config(ppx::ApplicationSettings& settings) override;
    virtual void Setup() override;
    virtual void Render() override;

private:
    struct RenderParameters
    {
        float4x4 PVM;
        float alpha;
    };

private:
    grfx::SemaphorePtr           mImageAcquiredSemaphore;
    grfx::FencePtr               mImageAcquiredFence;
    grfx::SemaphorePtr           mRenderCompleteSemaphore;
    grfx::FencePtr               mRenderCompleteFence;

    grfx::CommandBufferPtr       mCommandBuffer;

    grfx::DescriptorPoolPtr      mDescriptorPool;
    grfx::DescriptorSetLayoutPtr mDescriptorSetLayout;

    grfx::PipelineInterfacePtr   mPipelineInterface;
    grfx::GraphicsPipelinePtr    mPipeline;

    grfx::MeshPtr                mMonkeyMesh;
    grfx::DescriptorSetPtr       mDescriptorSet;
    grfx::BufferPtr              mUniformBuffer;
};

