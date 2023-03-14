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

void OITDemoApp::Config(ppx::ApplicationSettings& settings)
{
    settings.appName                    = "OIT demo";
    settings.enableImGui                = false;
#if defined(USE_DX11)
    settings.grfx.api                   = grfx::API_DX_11_1;
#elif defined(USE_DX12)
    settings.grfx.api                   = grfx::API_DX_12_0;
#elif defined(USE_VK)
    settings.grfx.api                   = grfx::API_VK_1_1;
#else
# error
#endif
    settings.grfx.enableDebug           = false;
#if defined(USE_DXIL)
    settings.grfx.enableDXIL            = true;
#endif
    settings.grfx.swapchain.depthFormat = grfx::FORMAT_D32_FLOAT;
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
        PPX_CHECKED_CALL(grfx_util::CreateMeshFromFile(queue, this->GetAssetPath("basic/models/monkey.obj"), &mMesh, options));

        grfx::BufferCreateInfo bufferCreateInfo        = {};
        bufferCreateInfo.size                          = PPX_MINIMUM_UNIFORM_BUFFER_SIZE;
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
        grfx::ShaderModulePtr VS, PS;
        PPX_CHECKED_CALL(CreateShader("basic/shaders", "VertexAutoColors.vs", &VS));
        PPX_CHECKED_CALL(CreateShader("basic/shaders", "VertexAutoColors.ps", &PS));

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
        piCreateInfo.setCount                          = 1;
        piCreateInfo.sets[0].set                       = 0;
        piCreateInfo.sets[0].pLayout                   = mDescriptorSetLayout;
        PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mPipelineInterface));

        grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
        gpCreateInfo.VS                                 = {VS, "vsmain"};
        gpCreateInfo.PS                                 = {PS, "psmain"};
        gpCreateInfo.vertexInputState.bindingCount      = 1;
        gpCreateInfo.vertexInputState.bindings[0]       = mMesh->GetDerivedVertexBindings()[0];
        gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
        gpCreateInfo.cullMode                           = grfx::CULL_MODE_BACK;
        gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
        gpCreateInfo.depthReadEnable                    = true;
        gpCreateInfo.depthWriteEnable                   = true;
        gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_NONE;
        gpCreateInfo.outputState.renderTargetCount      = 1;
        gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
        gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
        gpCreateInfo.pPipelineInterface                 = mPipelineInterface;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mPipeline));

        GetDevice()->DestroyShaderModule(VS);
        GetDevice()->DestroyShaderModule(PS);
    }

    // Per frame data
    {
        PPX_CHECKED_CALL(GetGraphicsQueue()->CreateCommandBuffer(&mFrame.cmd));

        grfx::SemaphoreCreateInfo semaCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mFrame.imageAcquiredSemaphore));

        grfx::FenceCreateInfo fenceCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mFrame.imageAcquiredFence));

        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mFrame.renderCompleteSemaphore));

        fenceCreateInfo.signaled = true;
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mFrame.renderCompleteFence));
    }
}

void OITDemoApp::Render()
{
    PerFrame& frame = mFrame;

    grfx::SwapchainPtr swapchain = GetSwapchain();

    uint32_t imageIndex = UINT32_MAX;
    PPX_CHECKED_CALL(swapchain->AcquireNextImage(UINT64_MAX, frame.imageAcquiredSemaphore, frame.imageAcquiredFence, &imageIndex));

    // Wait for and reset image acquired fence
    PPX_CHECKED_CALL(frame.imageAcquiredFence->WaitAndReset());

    // Wait for and reset render complete fence
    PPX_CHECKED_CALL(frame.renderCompleteFence->WaitAndReset());

    // Update uniform buffer
    {
        float    t = GetElapsedSeconds();
        float4x4 P = glm::perspective(glm::radians(60.0f), GetWindowAspect(), 0.001f, 10000.0f);
        float4x4 V = glm::lookAt(float3(0, 0, 8), float3(0, 0, 0), float3(0, 1, 0));
        float4x4 M = glm::rotate(t, float3(0, 0, 1)) * glm::rotate(2 * t, float3(0, 1, 0)) * glm::rotate(t, float3(1, 0, 0)) * glm::scale(float3(2));

        float4x4 T   = glm::translate(float3(0, 0, 0));
        float4x4 mat = P * V * T * M;
        mUniformBuffer->CopyFromSource(sizeof(mat), &mat);
    }

    // Build command buffer
    PPX_CHECKED_CALL(frame.cmd->Begin());
    {
        grfx::RenderPassPtr renderPass = swapchain->GetRenderPass(imageIndex);
        PPX_ASSERT_MSG(!renderPass.IsNull(), "render pass object is null");

        grfx::RenderPassBeginInfo beginInfo = {};
        beginInfo.pRenderPass               = renderPass;
        beginInfo.renderArea                = renderPass->GetRenderArea();
        beginInfo.RTVClearCount             = 1;
        beginInfo.RTVClearValues[0]         = {{0, 0, 0, 0}};
        beginInfo.DSVClearValue             = {1.0f, 0xFF};

        frame.cmd->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_PRESENT, grfx::RESOURCE_STATE_RENDER_TARGET);
        frame.cmd->BeginRenderPass(&beginInfo);
        {
            frame.cmd->SetScissors(GetScissor());
            frame.cmd->SetViewports(GetViewport());

            frame.cmd->BindGraphicsPipeline(mPipeline);
            frame.cmd->BindGraphicsDescriptorSets(mPipelineInterface, 1, &mDescriptorSet);
            frame.cmd->BindIndexBuffer(mMesh);
            frame.cmd->BindVertexBuffers(mMesh);
            frame.cmd->DrawIndexed(mMesh->GetIndexCount());

            // Draw ImGui
            DrawDebugInfo();
            DrawImGui(frame.cmd);
        }
        frame.cmd->EndRenderPass();
        frame.cmd->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    }
    PPX_CHECKED_CALL(frame.cmd->End());

    grfx::SubmitInfo submitInfo     = {};
    submitInfo.commandBufferCount   = 1;
    submitInfo.ppCommandBuffers     = &frame.cmd;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.ppWaitSemaphores     = &frame.imageAcquiredSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.ppSignalSemaphores   = &frame.renderCompleteSemaphore;
    submitInfo.pFence               = frame.renderCompleteFence;

    PPX_CHECKED_CALL(GetGraphicsQueue()->Submit(&submitInfo));

    PPX_CHECKED_CALL(swapchain->Present(imageIndex, 1, &frame.renderCompleteSemaphore));
}

