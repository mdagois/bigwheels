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

void OITDemoApp::Setup()
{
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

    // Set layout
    {
        grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{0, grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mDescriptorSetLayout));
    }

    // Models
    {
        grfx::QueuePtr queue = this->GetGraphicsQueue();
        TriMeshOptions options = TriMeshOptions().Indices();
        PPX_CHECKED_CALL(grfx_util::CreateMeshFromFile(queue, this->GetAssetPath("basic/models/cube.obj"), &mBackgroundMesh, options));
        PPX_CHECKED_CALL(grfx_util::CreateMeshFromFile(queue, this->GetAssetPath("basic/models/monkey.obj"), &mMonkeyMesh, options));
    }

    // Uniform buffer
    {
        grfx::BufferCreateInfo bufferCreateInfo        = {};
        bufferCreateInfo.size                          = std::max(sizeof(RenderParameters), static_cast<size_t>(PPX_MINIMUM_UNIFORM_BUFFER_SIZE));
        bufferCreateInfo.usageFlags.bits.uniformBuffer = true;
        bufferCreateInfo.memoryUsage                   = grfx::MEMORY_USAGE_CPU_TO_GPU;
        PPX_CHECKED_CALL(GetDevice()->CreateBuffer(&bufferCreateInfo, &mUniformBuffer));

        PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mDescriptorSetLayout, &mDescriptorSet));

        grfx::WriteDescriptor write = {};
        write.binding               = 0;
        write.type                  = grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.bufferOffset          = 0;
        write.bufferRange           = PPX_WHOLE_SIZE;
        write.pBuffer               = mUniformBuffer;
        PPX_CHECKED_CALL(mDescriptorSet->UpdateDescriptors(1, &write));
    }

    // Pipelines
    {
        {
            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Background.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Background.ps", &PS));

            grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
            piCreateInfo.setCount                          = 1;
            piCreateInfo.sets[0].set                       = 0;
            piCreateInfo.sets[0].pLayout                   = mDescriptorSetLayout;
            PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mPipelineInterface));

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
            gpCreateInfo.pPipelineInterface                 = mPipelineInterface;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mBackgroundPipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }

        {
            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlending.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlending.ps", &PS));

            grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
            piCreateInfo.setCount                          = 1;
            piCreateInfo.sets[0].set                       = 0;
            piCreateInfo.sets[0].pLayout                   = mDescriptorSetLayout;
            PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mPipelineInterface));

            grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
            gpCreateInfo.VS                                 = {VS, "vsmain"};
            gpCreateInfo.PS                                 = {PS, "psmain"};
            gpCreateInfo.vertexInputState.bindingCount      = 1;
            gpCreateInfo.vertexInputState.bindings[0]       = mMonkeyMesh->GetDerivedVertexBindings()[0];
            gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
            gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
            gpCreateInfo.depthReadEnable                    = true;
            gpCreateInfo.depthWriteEnable                   = true;
            gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_ALPHA;
            gpCreateInfo.outputState.renderTargetCount      = 1;
            gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
            gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
            gpCreateInfo.pPipelineInterface                 = mPipelineInterface;

            gpCreateInfo.cullMode = grfx::CULL_MODE_NONE;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mMeshAllFacesPipeline));

            gpCreateInfo.cullMode = grfx::CULL_MODE_FRONT;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mMeshBackFacesPipeline));

            gpCreateInfo.cullMode = grfx::CULL_MODE_BACK;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mMeshFrontFacesPipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }
    }

    {
        PPX_CHECKED_CALL(GetGraphicsQueue()->CreateCommandBuffer(&mCommandBuffer));
    }

    // Synchronization
    {
        grfx::SemaphoreCreateInfo semaCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mImageAcquiredSemaphore));

        grfx::FenceCreateInfo fenceCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mImageAcquiredFence));

        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mRenderCompleteSemaphore));

        fenceCreateInfo.signaled = true;
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mRenderCompleteFence));
    }
}

void OITDemoApp::Render()
{
    const float time = GetElapsedSeconds();
    grfx::SwapchainPtr swapchain = GetSwapchain();

    uint32_t imageIndex = UINT32_MAX;
    PPX_CHECKED_CALL(swapchain->AcquireNextImage(UINT64_MAX, mImageAcquiredSemaphore, mImageAcquiredFence, &imageIndex));
    PPX_CHECKED_CALL(mImageAcquiredFence->WaitAndReset());
    PPX_CHECKED_CALL(mRenderCompleteFence->WaitAndReset());

    // Uniform buffer update
    {
        const float4x4 VP =
            glm::perspective(glm::radians(60.0f), GetWindowAspect(), 0.001f, 10000.0f) *
            glm::lookAt(float3(0, 0, 8), float3(0, 0, 0), float3(0, 1, 0));

        RenderParameters renderParameters = {};
        {
            const float4x4 M = glm::scale(float3(3.0));
            renderParameters.backgroundMVP = VP * M;
        }
        {
            const float4x4 M =
                glm::rotate(time, float3(0, 0, 1)) *
                glm::rotate(2 * time, float3(0, 1, 0)) *
                glm::rotate(time, float3(1, 0, 0)) *
                glm::scale(float3(2));
            renderParameters.meshMVP = VP * M;
        }
        renderParameters.meshOpacity = mGuiParameters.meshOpacity;
        mUniformBuffer->CopyFromSource(sizeof(renderParameters), &renderParameters);
    }

    // Build command buffer
    PPX_CHECKED_CALL(mCommandBuffer->Begin());
    {
        grfx::RenderPassPtr renderPass = swapchain->GetRenderPass(imageIndex);
        PPX_ASSERT_MSG(!renderPass.IsNull(), "render pass object is null");

        grfx::RenderPassBeginInfo beginInfo = {};
        beginInfo.pRenderPass               = renderPass;
        beginInfo.renderArea                = renderPass->GetRenderArea();
        beginInfo.RTVClearCount             = 1;
        beginInfo.RTVClearValues[0]         = {{0, 0, 0, 0}};
        beginInfo.DSVClearValue             = {1.0f, 0xFF};

        mCommandBuffer->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_PRESENT, grfx::RESOURCE_STATE_RENDER_TARGET);
        mCommandBuffer->BeginRenderPass(&beginInfo);
        {
            mCommandBuffer->SetScissors(GetScissor());
            mCommandBuffer->SetViewports(GetViewport());
            mCommandBuffer->BindGraphicsDescriptorSets(mPipelineInterface, 1, &mDescriptorSet);
        }

        {
            mCommandBuffer->BindGraphicsPipeline(mBackgroundPipeline);
            mCommandBuffer->BindIndexBuffer(mBackgroundMesh);
            mCommandBuffer->BindVertexBuffers(mBackgroundMesh);
            mCommandBuffer->DrawIndexed(mBackgroundMesh->GetIndexCount());
        }

        mCommandBuffer->BindIndexBuffer(mMonkeyMesh);
        mCommandBuffer->BindVertexBuffers(mMonkeyMesh);
        switch(mGuiParameters.faceMode)
        {
            case FACE_MODE_ALL:
            {
                mCommandBuffer->BindGraphicsPipeline(mMeshAllFacesPipeline);
                mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
                break;
            }
            case FACE_MODE_ALL_BACK_THEN_FRONT:
            {
                mCommandBuffer->BindGraphicsPipeline(mMeshBackFacesPipeline);
                mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
                mCommandBuffer->BindGraphicsPipeline(mMeshFrontFacesPipeline);
                mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
                break;
            }
            case FACE_MODE_BACK_ONLY:
            {
                mCommandBuffer->BindGraphicsPipeline(mMeshBackFacesPipeline);
                mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
                break;
            }
            case FACE_MODE_FRONT_ONLY:
            {
                mCommandBuffer->BindGraphicsPipeline(mMeshFrontFacesPipeline);
                mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());
                break;
            }
            default:
            {
                PPX_ASSERT_MSG(false, "unknown face mode");
                break;
            }
        }

        {
            if(ImGui::Begin("Parameters"))
            {
                ImGui::SliderFloat("Opacity", &mGuiParameters.meshOpacity, 0.0f, 1.0f, "%.2f");

                const char* faceModes[] =
                {
                    "All",
                    "Back first, then front",
                    "Back only",
                    "Front only",
                };
                static_assert(IM_ARRAYSIZE(faceModes) == FACE_MODES_COUNT);
                ImGui::Combo("Face draw mode", reinterpret_cast<int32_t*>(&mGuiParameters.faceMode), faceModes, IM_ARRAYSIZE(faceModes));
            }
            ImGui::End();
            DrawImGui(mCommandBuffer);
        }
        mCommandBuffer->EndRenderPass();
        mCommandBuffer->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    }
    PPX_CHECKED_CALL(mCommandBuffer->End());

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
    PPX_CHECKED_CALL(swapchain->Present(imageIndex, 1, &mRenderCompleteSemaphore));
}

