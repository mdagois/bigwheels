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

#include "XlsViewer.h"

//TODO Control clear color
//TODO Control real size/fit to screen/etc.

void XlsViewerApp::Config(ppx::ApplicationSettings& settings)
{
    settings.appName          = "xls_viewer";

    settings.enableImGui      = false;
    settings.grfx.enableDebug = false;

#if defined(USE_DX11)
    settings.grfx.api         = grfx::API_DX_11_1;
#elif defined(USE_DX12)
    settings.grfx.api         = grfx::API_DX_12_0;
#elif defined(USE_VK)
    settings.grfx.api         = grfx::API_VK_1_1;
#endif
#if defined(USE_DXIL)
    settings.grfx.enableDXIL = true;
#endif
}

void XlsViewerApp::Setup()
{
    {
        grfx::SemaphoreCreateInfo semaCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mImageAcquiredSemaphore));
        PPX_CHECKED_CALL(GetDevice()->CreateSemaphore(&semaCreateInfo, &mRenderCompleteSemaphore));

        grfx::FenceCreateInfo fenceCreateInfo = {};
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mImageAcquiredFence));
        fenceCreateInfo = {true};
        PPX_CHECKED_CALL(GetDevice()->CreateFence(&fenceCreateInfo, &mRenderCompleteFence));
    }

    {
		PPX_CHECKED_CALL(GetGraphicsQueue()->CreateCommandBuffer(&mCommandBuffer));

        grfx::DescriptorPoolCreateInfo createInfo = {};
        createInfo.sampler                        = 16;
        createInfo.sampledImage                   = 16;
        createInfo.uniformBuffer                  = 16;
        createInfo.structuredBuffer               = 16;
        createInfo.storageTexelBuffer             = 16;
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorPool(&createInfo, &mDescriptorPool));
    }

    {
        grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{0, grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mScreenDescriptorSetLayout));
    }

    {
        grfx::BufferCreateInfo bufferCreateInfo        = {};
        bufferCreateInfo.size                          = std::max(sizeof(ShaderGlobals), static_cast<size_t>(PPX_MINIMUM_UNIFORM_BUFFER_SIZE));
        bufferCreateInfo.usageFlags.bits.uniformBuffer = true;
        bufferCreateInfo.memoryUsage                   = grfx::MEMORY_USAGE_CPU_TO_GPU;
        PPX_CHECKED_CALL(GetDevice()->CreateBuffer(&bufferCreateInfo, &mScreenUniformBuffer));

        PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mScreenDescriptorSetLayout, &mScreenDescriptorSet));

        grfx::WriteDescriptor write = {};
        write.binding               = 0;
        write.type                  = grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.bufferOffset          = 0;
        write.bufferRange           = PPX_WHOLE_SIZE;
        write.pBuffer               = mScreenUniformBuffer;
        PPX_CHECKED_CALL(mScreenDescriptorSet->UpdateDescriptors(1, &write));
    }

	{
        grfx::BufferCreateInfo bufferCreateInfo           = {};
        bufferCreateInfo.size                             = GRAPHICS_BUFFER_SIZE;
        bufferCreateInfo.structuredElementStride          = BYTES_PER_PIXEL;
        bufferCreateInfo.usageFlags.bits.structuredBuffer = true;
        bufferCreateInfo.memoryUsage                      = grfx::MEMORY_USAGE_CPU_TO_GPU;
        bufferCreateInfo.initialState                     = grfx::RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        PPX_CHECKED_CALL(GetDevice()->CreateBuffer(&bufferCreateInfo, &mScreenPixelBuffer));

        void* pAddress = nullptr;
        PPX_CHECKED_CALL(mScreenPixelBuffer->MapMemory(0, &pAddress));
        memset(pAddress, 0, GRAPHICS_BUFFER_SIZE);
        mScreenPixelBuffer->UnmapMemory();
	}

    {
		ppx::grfx::ShaderModulePtr VS, PS;
		PPX_CHECKED_CALL(CreateShader("xls_viewer/shaders", "Screen.vs", &VS));
		PPX_CHECKED_CALL(CreateShader("xls_viewer/shaders", "Screen.ps", &PS));

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
		piCreateInfo.setCount                          = 1;
		piCreateInfo.sets[0].set                       = 0;
		piCreateInfo.sets[0].pLayout                   = mScreenDescriptorSetLayout;
        PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mScreenPipelineInterface));

        grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
        gpCreateInfo.VS                                 = {VS.Get(), "vsmain"};
        gpCreateInfo.PS                                 = {PS.Get(), "psmain"};
        gpCreateInfo.vertexInputState.bindingCount      = 0;
        gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
        gpCreateInfo.cullMode                           = grfx::CULL_MODE_BACK;
        gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
        gpCreateInfo.depthReadEnable                    = false;
        gpCreateInfo.depthWriteEnable                   = false;
        gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_NONE;
        gpCreateInfo.outputState.renderTargetCount      = 1;
        gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
        gpCreateInfo.pPipelineInterface                 = mScreenPipelineInterface;
        PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mScreenPipeline));

		GetDevice()->DestroyShaderModule(VS);
		GetDevice()->DestroyShaderModule(PS);
    }
}

void XlsViewerApp::RecordCommandBuffer(uint32_t imageIndex)
{
    grfx::RenderPassPtr renderPass = GetSwapchain()->GetRenderPass(imageIndex);
    PPX_ASSERT_MSG(!renderPass.IsNull(), "render pass object is null");

    PPX_CHECKED_CALL(mCommandBuffer->Begin());
    mCommandBuffer->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_PRESENT, grfx::RESOURCE_STATE_RENDER_TARGET);

    grfx::RenderPassBeginInfo beginInfo = {};
    beginInfo.pRenderPass               = renderPass;
    beginInfo.renderArea                = renderPass->GetRenderArea();
    beginInfo.RTVClearCount             = 1;
    beginInfo.RTVClearValues[0]         = {{0, 0, 0, 1}};
    mCommandBuffer->BeginRenderPass(&beginInfo);

    mCommandBuffer->SetScissors(renderPass->GetScissor());
    mCommandBuffer->SetViewports(renderPass->GetViewport());

    {
        //TODO Map a constant buffer with width/height of render + resolution width/height
        //TODO Update the content of the updated texture
        mCommandBuffer->BindGraphicsDescriptorSets(mScreenPipelineInterface, 1, &mScreenDescriptorSet);
        mCommandBuffer->BindGraphicsPipeline(mScreenPipeline);
        mCommandBuffer->Draw(6, 1, 0, 0);
    }

    DrawDebugInfo();
    DrawImGui(mCommandBuffer);

    mCommandBuffer->EndRenderPass();

    mCommandBuffer->TransitionImageLayout(renderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    PPX_CHECKED_CALL(mCommandBuffer->End());
}

void XlsViewerApp::Render()
{
    //TODO Call the texture update function as much as necessary

    uint32_t imageIndex = UINT32_MAX;
    PPX_CHECKED_CALL(GetSwapchain()->AcquireNextImage(UINT64_MAX, mImageAcquiredSemaphore, mImageAcquiredFence, &imageIndex));
    PPX_CHECKED_CALL(mImageAcquiredFence->WaitAndReset());
    PPX_CHECKED_CALL(mRenderCompleteFence->WaitAndReset());

	ShaderGlobals shaderGlobals = {};
	//TODO Properly set these
	shaderGlobals.Resolution.x = 1280.0f;
	shaderGlobals.Resolution.y = 720.0f;
	shaderGlobals.Resolution.w = 600.0f;
	shaderGlobals.Resolution.z = 600.0f;
	mScreenUniformBuffer->CopyFromSource(sizeof(ShaderGlobals), &shaderGlobals);

    RecordCommandBuffer(imageIndex);

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

