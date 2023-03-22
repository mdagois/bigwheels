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

//TODO Draw meshkin
//TODO Control clear color from ImGui
//TODO Choice of cubemaps as background

#include "OITDemoApplication.h"
#include "ppx/graphics_util.h"
#include "shaders/Common.hlsli"

void OITDemoApp::Config(ppx::ApplicationSettings& settings)
{
    settings.appName                    = "OIT demo";
    settings.enableImGui                = true;
    settings.grfx.enableDebug           = false;
    settings.grfx.swapchain.depthFormat = grfx::FORMAT_D32_FLOAT;

#if defined(USE_DX11)
    settings.grfx.api                   = grfx::API_DX_11_1;
#elif defined(USE_DX12)
    settings.grfx.api                   = grfx::API_DX_12_0;
#elif defined(USE_VK)
    settings.grfx.api                   = grfx::API_VK_1_1;
#else
# error
#endif
#if defined(USE_DXIL)
    settings.grfx.enableDXIL            = true;
#endif
}

void OITDemoApp::SetupCommon()
{
    // Synchronization objects
    {
        grfx::SemaphoreCreateInfo semaCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mImageAcquiredSemaphore));

        grfx::FenceCreateInfo fenceCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mImageAcquiredFence));

        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mRenderCompleteSemaphore));

        fenceCreateInfo.signaled = true;
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mRenderCompleteFence));
    }

    // Command buffer
    {
        PPX_CHECKED_CALL(GetGraphicsQueue()->CreateCommandBuffer(&mCommandBuffer));
    }

    // Descriptor pool
    {
        grfx::DescriptorPoolCreateInfo createInfo = {};
        createInfo.sampler                        = 16;
        createInfo.sampledImage                   = 16;
        createInfo.uniformBuffer                  = 16;
        createInfo.structuredBuffer               = 16;
        createInfo.storageTexelBuffer             = 16;
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorPool(&createInfo, &mDescriptorPool));
    }

    // Meshes
    {
        grfx::QueuePtr queue = this->GetGraphicsQueue();
        TriMeshOptions options = TriMeshOptions().Indices();
        PPX_CHECKED_CALL(grfx_util::CreateMeshFromFile(queue, this->GetAssetPath("basic/models/cube.obj"), &mBackgroundMesh, options));
        PPX_CHECKED_CALL(grfx_util::CreateMeshFromFile(queue, this->GetAssetPath("basic/models/monkey.obj"), &mMonkeyMesh, options));
    }

    // Shader globals
    {
        grfx::BufferCreateInfo bufferCreateInfo        = {};
        bufferCreateInfo.size                          = std::max(sizeof(ShaderGlobals), static_cast<size_t>(PPX_MINIMUM_UNIFORM_BUFFER_SIZE));
        bufferCreateInfo.usageFlags.bits.uniformBuffer = true;
        bufferCreateInfo.memoryUsage                   = grfx::MEMORY_USAGE_CPU_TO_GPU;
        PPX_CHECKED_CALL(GetDevice()->CreateBuffer(&bufferCreateInfo, &mShaderGlobalsBuffer));
    }

    // Samplers
    {
        grfx::SamplerCreateInfo createInfo = {};
        createInfo.magFilter               = grfx::FILTER_NEAREST;
        createInfo.minFilter               = grfx::FILTER_NEAREST;
        PPX_CHECKED_CALL(GetDevice()->CreateSampler(&createInfo, &mComposeSampler));
    }
}

void OITDemoApp::SetupBackground()
{
    // Descriptor
    {
        grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{SHADER_GLOBALS_REGISTER, grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mBackground.descriptorSetLayout));

        PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mBackground.descriptorSetLayout, &mBackground.descriptorSet));

        grfx::WriteDescriptor write = {};
        write.binding = SHADER_GLOBALS_REGISTER;
        write.type = grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.bufferOffset = 0;
        write.bufferRange = PPX_WHOLE_SIZE;
        write.pBuffer = mShaderGlobalsBuffer;
        PPX_CHECKED_CALL(mBackground.descriptorSet->UpdateDescriptors(1, &write));
    }

    // Pipeline
    {
        grfx::ShaderModulePtr VS, PS;
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Background.vs", &VS));
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Background.ps", &PS));

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
        piCreateInfo.setCount                          = 1;
        piCreateInfo.sets[0].set                       = 0;
        piCreateInfo.sets[0].pLayout                   = mBackground.descriptorSetLayout;
        PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mBackground.pipelineInterface));

        grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
        gpCreateInfo.VS                                 = {VS, "vsmain"};
        gpCreateInfo.PS                                 = {PS, "psmain"};
        gpCreateInfo.vertexInputState.bindingCount      = 1;
        gpCreateInfo.vertexInputState.bindings[0]       = mMonkeyMesh->GetDerivedVertexBindings()[0];
        gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
        gpCreateInfo.cullMode                           = grfx::CULL_MODE_FRONT;
        gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
        gpCreateInfo.depthReadEnable                    = true;
        gpCreateInfo.depthWriteEnable                   = true;
        gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_NONE;
        gpCreateInfo.outputState.renderTargetCount      = 1;
        gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
        gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
        gpCreateInfo.pPipelineInterface                 = mBackground.pipelineInterface;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mBackground.pipeline));

        GetDevice()->DestroyShaderModule(VS);
        GetDevice()->DestroyShaderModule(PS);
    }
}

void OITDemoApp::SetupAlphaBlending()
{
    // Descriptor
    {
        mAlphaBlending.descriptorSetLayout = mBackground.descriptorSetLayout;
        mAlphaBlending.descriptorSet = mBackground.descriptorSet;
        mAlphaBlending.pipelineInterface = mBackground.pipelineInterface;
    }

    // Pipeline
    {
        grfx::ShaderModulePtr VS, PS;
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlending.vs", &VS));
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlending.ps", &PS));

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
        piCreateInfo.setCount                          = 1;
        piCreateInfo.sets[0].set                       = 0;
        piCreateInfo.sets[0].pLayout                   = mBackground.descriptorSetLayout;
        PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mAlphaBlending.pipelineInterface));

        grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
        gpCreateInfo.VS                                 = {VS, "vsmain"};
        gpCreateInfo.PS                                 = {PS, "psmain"};
        gpCreateInfo.vertexInputState.bindingCount      = 1;
        gpCreateInfo.vertexInputState.bindings[0]       = mMonkeyMesh->GetDerivedVertexBindings()[0];
        gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
        gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
        gpCreateInfo.depthReadEnable                    = true;
        gpCreateInfo.depthWriteEnable                   = false;
        gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_ALPHA;
        gpCreateInfo.outputState.renderTargetCount      = 1;
        gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
        gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
        gpCreateInfo.pPipelineInterface                 = mAlphaBlending.pipelineInterface;

        gpCreateInfo.cullMode = grfx::CULL_MODE_NONE;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mAlphaBlending.meshAllFacesPipeline));

        gpCreateInfo.cullMode = grfx::CULL_MODE_FRONT;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mAlphaBlending.meshBackFacesPipeline));

        gpCreateInfo.cullMode = grfx::CULL_MODE_BACK;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mAlphaBlending.meshFrontFacesPipeline));

        GetDevice()->DestroyShaderModule(VS);
        GetDevice()->DestroyShaderModule(PS);
    }
}

void OITDemoApp::SetupMeshkin()
{
    // Render pass
    {
        grfx::DrawPassCreateInfo createInfo     = {};
        createInfo.width                        = GetWindowWidth();
        createInfo.height                       = GetWindowHeight();
        createInfo.renderTargetCount            = 1;
        createInfo.renderTargetFormats[0]       = grfx::FORMAT_B8G8R8A8_UNORM;
        createInfo.depthStencilFormat           = grfx::FORMAT_D32_FLOAT;
        createInfo.renderTargetUsageFlags[0]    = grfx::IMAGE_USAGE_SAMPLED;
        createInfo.renderTargetInitialStates[0] = grfx::RESOURCE_STATE_RENDER_TARGET;
        createInfo.depthStencilInitialState     = grfx::RESOURCE_STATE_DEPTH_STENCIL_READ;
        createInfo.renderTargetClearValues[0]   = {0, 0, 0, 0};
        PPX_CHECKED_CALL(GetDevice()->CreateDrawPass(&createInfo, &mMeshkin.transparencyPass));
    }

    {
        grfx::TextureCreateInfo createInfo         = {};
        createInfo.imageType                       = grfx::IMAGE_TYPE_2D;
        createInfo.width                           = GetWindowWidth();
        createInfo.height                          = GetWindowHeight();
        createInfo.depth                           = 1;
        createInfo.imageFormat                     = grfx::FORMAT_R8G8B8A8_UNORM;
        createInfo.sampleCount                     = grfx::SAMPLE_COUNT_1;
        createInfo.mipLevelCount                   = 1;
        createInfo.arrayLayerCount                 = 1;
        createInfo.usageFlags.bits.colorAttachment = true;
        createInfo.usageFlags.bits.sampled         = true;
        createInfo.memoryUsage                     = grfx::MEMORY_USAGE_GPU_ONLY;
        createInfo.initialState                    = grfx::RESOURCE_STATE_RENDER_TARGET;
        createInfo.RTVClearValue                   = {0, 0, 0, 0};
        createInfo.DSVClearValue                   = {1.0f, 0xFF};

        PPX_CHECKED_CALL(GetDevice()->CreateTexture(&createInfo, &mMeshkin.transparencyTexture));
    }
}

void OITDemoApp::Setup()
{
    SetupCommon();
    SetupBackground();
    SetupAlphaBlending();
    SetupMeshkin();
}

void OITDemoApp::DrawBackground()
{
    if(!mGuiParameters.displayBackground)
    {
        return;
    }
    mCommandBuffer->BindGraphicsDescriptorSets(mBackground.pipelineInterface, 1, &mBackground.descriptorSet);
    mCommandBuffer->BindGraphicsPipeline(mBackground.pipeline);
    mCommandBuffer->BindIndexBuffer(mBackgroundMesh);
    mCommandBuffer->BindVertexBuffers(mBackgroundMesh);
    mCommandBuffer->DrawIndexed(mBackgroundMesh->GetIndexCount());
}

void OITDemoApp::DrawGui()
{
    if(ImGui::Begin("Parameters"))
    {
        const char* algorithmChoices[] =
        {
            "Alpha blending",
            "Meshkin",
        };
        static_assert(IM_ARRAYSIZE(algorithmChoices) == ALGORITHMS_COUNT, "Algorithm count mismatch");
        ImGui::Combo("Algorithm", reinterpret_cast<int32_t*>(&mGuiParameters.algorithm), algorithmChoices, IM_ARRAYSIZE(algorithmChoices));

        ImGui::SliderFloat("Opacity", &mGuiParameters.meshOpacity, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Display background", &mGuiParameters.displayBackground);

        ImGui::Separator();

        switch(mGuiParameters.algorithm)
        {
            case ALGORITHM_ALPHA_BLENDING:
            {
                const char* faceModeChoices[] =
                {
                    "All",
                    "Back first, then front",
                    "Back only",
                    "Front only",
                };
                static_assert(IM_ARRAYSIZE(faceModeChoices) == FACE_MODES_COUNT, "Face modes count mismatch");
                ImGui::Combo("Face draw mode", reinterpret_cast<int32_t*>(&mGuiParameters.faceMode), faceModeChoices, IM_ARRAYSIZE(faceModeChoices));
                break;
            }
            default:
            {
                break;
            }
        }
    }
    ImGui::End();
    DrawImGui(mCommandBuffer);
}

void OITDemoApp::RecordAlphaBlending(grfx::RenderPassPtr finalRenderPass)
{
    PPX_CHECKED_CALL(mCommandBuffer->Begin());
    mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_PRESENT, grfx::RESOURCE_STATE_RENDER_TARGET);

    grfx::RenderPassBeginInfo beginInfo = {};
    beginInfo.pRenderPass               = finalRenderPass;
    beginInfo.renderArea                = finalRenderPass->GetRenderArea();
    beginInfo.RTVClearCount             = 1;
    beginInfo.RTVClearValues[0]         = {{0, 0, 0, 0}};
    beginInfo.DSVClearValue             = {1.0f, 0xFF};
    mCommandBuffer->BeginRenderPass(&beginInfo);

    mCommandBuffer->SetScissors(GetScissor());
    mCommandBuffer->SetViewports(GetViewport());

    DrawBackground();

    mCommandBuffer->BindGraphicsDescriptorSets(mAlphaBlending.pipelineInterface, 1, &mAlphaBlending.descriptorSet);
    mCommandBuffer->BindIndexBuffer(mMonkeyMesh);
    mCommandBuffer->BindVertexBuffers(mMonkeyMesh);
    switch(mGuiParameters.faceMode)
    {
        case FACE_MODE_ALL:
        {
            mCommandBuffer->BindGraphicsPipeline(mAlphaBlending.meshAllFacesPipeline);
            mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
            break;
        }
        case FACE_MODE_ALL_BACK_THEN_FRONT:
        {
            mCommandBuffer->BindGraphicsPipeline(mAlphaBlending.meshBackFacesPipeline);
            mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
            mCommandBuffer->BindGraphicsPipeline(mAlphaBlending.meshFrontFacesPipeline);
            mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
            break;
        }
        case FACE_MODE_BACK_ONLY:
        {
            mCommandBuffer->BindGraphicsPipeline(mAlphaBlending.meshBackFacesPipeline);
            mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
            break;
        }
        case FACE_MODE_FRONT_ONLY:
        {
            mCommandBuffer->BindGraphicsPipeline(mAlphaBlending.meshFrontFacesPipeline);
            mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
            break;
        }
        default:
        {
            PPX_ASSERT_MSG(false, "unknown face mode");
            break;
        }
    }

    DrawGui();

    mCommandBuffer->EndRenderPass();
    mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    PPX_CHECKED_CALL(mCommandBuffer->End());
}

void OITDemoApp::RecordMeshkin(grfx::RenderPassPtr finalRenderPass)
{
    PPX_CHECKED_CALL(mCommandBuffer->Begin());
    mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_PRESENT, grfx::RESOURCE_STATE_RENDER_TARGET);

    grfx::RenderPassBeginInfo beginInfo = {};
    beginInfo.pRenderPass               = finalRenderPass;
    beginInfo.renderArea                = finalRenderPass->GetRenderArea();
    beginInfo.RTVClearCount             = 1;
    beginInfo.RTVClearValues[0]         = {{0, 0, 0, 0}};
    beginInfo.DSVClearValue             = {1.0f, 0xFF};
    mCommandBuffer->BeginRenderPass(&beginInfo);

    mCommandBuffer->SetScissors(GetScissor());
    mCommandBuffer->SetViewports(GetViewport());

    DrawBackground();
    DrawGui();

    mCommandBuffer->EndRenderPass();
    mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    PPX_CHECKED_CALL(mCommandBuffer->End());
}

void OITDemoApp::Render()
{
    uint32_t imageIndex = UINT32_MAX;
    PPX_CHECKED_CALL(GetSwapchain()->AcquireNextImage(UINT64_MAX, mImageAcquiredSemaphore, mImageAcquiredFence, &imageIndex));
    PPX_CHECKED_CALL(mImageAcquiredFence->WaitAndReset());
    PPX_CHECKED_CALL(mRenderCompleteFence->WaitAndReset());

    // Uniform buffer update
    {
        const float time = GetElapsedSeconds();

        const float4x4 VP =
            glm::perspective(glm::radians(60.0f), GetWindowAspect(), 0.001f, 10000.0f) *
            glm::lookAt(float3(0, 0, 8), float3(0, 0, 0), float3(0, 1, 0));

        ShaderGlobals shaderGlobals = {};
        {
            const float4x4 M = glm::scale(float3(3.0));
            shaderGlobals.backgroundMVP = VP * M;
        }
        {
            const float4x4 M =
                glm::rotate(time, float3(0, 0, 1)) *
                glm::rotate(2 * time, float3(0, 1, 0)) *
                glm::rotate(time, float3(1, 0, 0)) *
                glm::scale(float3(2));
            shaderGlobals.meshMVP = VP * M;
        }
        shaderGlobals.meshOpacity = mGuiParameters.meshOpacity;
        mShaderGlobalsBuffer->CopyFromSource(sizeof(shaderGlobals), &shaderGlobals);
    }

    // Record command buffer
    grfx::RenderPassPtr finalRenderPass = GetSwapchain()->GetRenderPass(imageIndex);
    PPX_ASSERT_MSG(!finalRenderPass.IsNull(), "render pass object is null");
    switch(mGuiParameters.algorithm)
    {
        case ALGORITHM_ALPHA_BLENDING:
            RecordAlphaBlending(finalRenderPass);
            break;
        case ALGORITHM_MESHKIN:
            RecordMeshkin(finalRenderPass);
            break;
        default:
            PPX_ASSERT_MSG(false, "unknown algorithm");
            break;
    }

    // Submit and present
    grfx::SubmitInfo submitInfo     = {};
    submitInfo.commandBufferCount   = 1;
    submitInfo.ppCommandBuffers     = &mCommandBuffer;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.ppWaitSemaphores     = &mImageAcquiredSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.ppSignalSemaphores   = &mRenderCompleteSemaphore;
    submitInfo.pFence               = mRenderCompleteFence;
    PPX_CHECKED_CALL(GetGraphicsQueue()->Submit(&submitInfo));
    PPX_CHECKED_CALL(GetSwapchain()->Present(imageIndex, 1, &mRenderCompleteSemaphore));
}

