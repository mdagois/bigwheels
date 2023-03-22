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
    enum Algorithm : int32_t
    {
        ALGORITHM_ALPHA_BLENDING,
        ALGORITHM_MESHKIN,
        ALGORITHMS_COUNT,
    };

    enum FaceMode : int32_t
    {
        FACE_MODE_ALL,
        FACE_MODE_ALL_BACK_THEN_FRONT,
        FACE_MODE_BACK_ONLY,
        FACE_MODE_FRONT_ONLY,
        FACE_MODES_COUNT,
    };

    struct GuiParameters
    {
        GuiParameters()
        : meshOpacity(1.0f)
        , algorithm(ALGORITHM_ALPHA_BLENDING)
        , faceMode(FACE_MODE_ALL)
        , displayBackground(true)
        {
        }

        float meshOpacity;
        Algorithm algorithm;
        FaceMode faceMode;
        bool displayBackground;
    };

private:
    void SetupBackground();
    void SetupAlphaBlending();
    void SetupMeshkin();

    void DrawBackground();
    void DrawGui();

    void RecordAlphaBlending(grfx::RenderPassPtr finalRenderPass);
    void RecordMeshkin(grfx::RenderPassPtr finalRenderPass);

private:
    GuiParameters                mGuiParameters;

    grfx::SemaphorePtr           mImageAcquiredSemaphore;
    grfx::FencePtr               mImageAcquiredFence;
    grfx::SemaphorePtr           mRenderCompleteSemaphore;
    grfx::FencePtr               mRenderCompleteFence;

    grfx::CommandBufferPtr       mCommandBuffer;
    grfx::DescriptorPoolPtr      mDescriptorPool;
    grfx::MeshPtr                mBackgroundMesh;
    grfx::MeshPtr                mMonkeyMesh;
    grfx::BufferPtr              mShaderGlobalsBuffer;

    struct
    {
        grfx::DescriptorSetLayoutPtr descriptorSetLayout;
        grfx::DescriptorSetPtr       descriptorSet;
        grfx::PipelineInterfacePtr   pipelineInterface;
        grfx::GraphicsPipelinePtr    pipeline;
    } mBackground;

    struct
    {
        grfx::DescriptorSetLayoutPtr descriptorSetLayout;
        grfx::DescriptorSetPtr       descriptorSet;
        grfx::PipelineInterfacePtr   pipelineInterface;
        grfx::GraphicsPipelinePtr    meshAllFacesPipeline;
        grfx::GraphicsPipelinePtr    meshBackFacesPipeline;
        grfx::GraphicsPipelinePtr    meshFrontFacesPipeline;
    } mAlphaBlending;
};

