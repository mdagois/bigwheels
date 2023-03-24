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

//TODO Cleanup objects
//TODO Control clear color from ImGui
//TODO Choice of cubemaps as background

#include "OITDemoApplication.h"
#include "ppx/graphics_util.h"
#include "shaders/Common.hlsli"

void OITDemoApp::Config(ppx::ApplicationSettings& settings)
{
    settings.appName                    = "OIT demo";

    settings.enableImGui                = true;
    settings.grfx.enableDebug           = true;

    settings.grfx.swapchain.colorFormat = grfx::FORMAT_B8G8R8A8_UNORM;

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

    // Opaque
    {
        // Pass
        {
            grfx::DrawPassCreateInfo createInfo     = {};
            createInfo.width                        = GetSwapchain()->GetWidth();
            createInfo.height                       = GetSwapchain()->GetHeight();
            createInfo.renderTargetCount            = 1;
            createInfo.renderTargetFormats[0]       = GetSwapchain()->GetColorFormat();
            createInfo.depthStencilFormat           = grfx::FORMAT_D32_FLOAT;
            createInfo.renderTargetUsageFlags[0]    = grfx::IMAGE_USAGE_SAMPLED;
            createInfo.depthStencilUsageFlags       = grfx::IMAGE_USAGE_SAMPLED;
            createInfo.renderTargetInitialStates[0] = grfx::RESOURCE_STATE_SHADER_RESOURCE;
            createInfo.depthStencilInitialState     = grfx::RESOURCE_STATE_SHADER_RESOURCE;
            createInfo.renderTargetClearValues[0]   = {0, 0, 0, 0};
            createInfo.depthStencilClearValue       = {1.0f, 0xFF};
            PPX_CHECKED_CALL(GetDevice()->CreateDrawPass(&createInfo, &mOpaquePass));
        }

        // Descriptor
        {
            grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
            layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{SHADER_GLOBALS_REGISTER, grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
            PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mOpaqueDescriptorSetLayout));

            PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mOpaqueDescriptorSetLayout, &mOpaqueDescriptorSet));

            grfx::WriteDescriptor write = {};
            write.binding = SHADER_GLOBALS_REGISTER;
            write.type = grfx::DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.bufferOffset = 0;
            write.bufferRange = PPX_WHOLE_SIZE;
            write.pBuffer = mShaderGlobalsBuffer;
            PPX_CHECKED_CALL(mOpaqueDescriptorSet->UpdateDescriptors(1, &write));
        }

        // Pipeline
        {
            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Opaque.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Opaque.ps", &PS));

            grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
            piCreateInfo.setCount                          = 1;
            piCreateInfo.sets[0].set                       = 0;
            piCreateInfo.sets[0].pLayout                   = mOpaqueDescriptorSetLayout;
            PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mOpaquePipelineInterface));

            grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
            gpCreateInfo.VS                                 = {VS, "vsmain"};
            gpCreateInfo.PS                                 = {PS, "psmain"};
            gpCreateInfo.vertexInputState.bindingCount      = 1;
            gpCreateInfo.vertexInputState.bindings[0]       = mBackgroundMesh->GetDerivedVertexBindings()[0];
            gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
            gpCreateInfo.cullMode                           = grfx::CULL_MODE_FRONT;
            gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
            gpCreateInfo.depthReadEnable                    = true;
            gpCreateInfo.depthWriteEnable                   = true;
            gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_NONE;
            gpCreateInfo.outputState.renderTargetCount      = 1;
            gpCreateInfo.outputState.renderTargetFormats[0] = mOpaquePass->GetRenderTargetTexture(0)->GetImageFormat();
            gpCreateInfo.outputState.depthStencilFormat     = mOpaquePass->GetDepthStencilTexture()->GetImageFormat();
            gpCreateInfo.pPipelineInterface                 = mOpaquePipelineInterface;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mOpaquePipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }
    }

    // Transparency
    {
        // Texture
        {
            grfx::TextureCreateInfo createInfo         = {};
            createInfo.imageType                       = grfx::IMAGE_TYPE_2D;
            createInfo.width                           = GetSwapchain()->GetWidth();
            createInfo.height                          = GetSwapchain()->GetHeight();
            createInfo.depth                           = 1;
            createInfo.imageFormat                     = grfx::FORMAT_B8G8R8A8_UNORM;
            createInfo.sampleCount                     = grfx::SAMPLE_COUNT_1;
            createInfo.mipLevelCount                   = 1;
            createInfo.arrayLayerCount                 = 1;
            createInfo.usageFlags.bits.colorAttachment = true;
            createInfo.usageFlags.bits.sampled         = true;
            createInfo.memoryUsage                     = grfx::MEMORY_USAGE_GPU_ONLY;
            createInfo.initialState                    = grfx::RESOURCE_STATE_SHADER_RESOURCE;
            createInfo.RTVClearValue                   = {0, 0, 0, 0};
            createInfo.DSVClearValue                   = {1.0f, 0xFF};
            PPX_CHECKED_CALL(GetDevice()->CreateTexture(&createInfo, &mTransparencyTexture));
        }

        // Pass
        {
            grfx::DrawPassCreateInfo2 createInfo       = {};
            createInfo.width                           = GetSwapchain()->GetWidth();
            createInfo.height                          = GetSwapchain()->GetHeight();
            createInfo.renderTargetCount               = 1;
            createInfo.pRenderTargetImages[0]          = mTransparencyTexture->GetImage();
            createInfo.pDepthStencilImage              = mOpaquePass->GetDepthStencilTexture()->GetImage();
            createInfo.depthStencilState               = grfx::RESOURCE_STATE_DEPTH_STENCIL_READ;
            createInfo.renderTargetClearValues[0]      = {0, 0, 0, 0};
            createInfo.depthStencilClearValue          = {1.0f, 0xFF};
            PPX_CHECKED_CALL(GetDevice()->CreateDrawPass(&createInfo, &mTransparencyPass));
        }
    }

    // Composition
    {
        // Sampler
        {
            grfx::SamplerCreateInfo createInfo = {};
            createInfo.magFilter               = grfx::FILTER_NEAREST;
            createInfo.minFilter               = grfx::FILTER_NEAREST;
            PPX_CHECKED_CALL(GetDevice()->CreateSampler(&createInfo, &mCompositionSampler));
        }

        // Descriptor
        {
            grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
            layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{COMPOSITION_SAMPLER_REGISTER, grfx::DESCRIPTOR_TYPE_SAMPLER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
            layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{OPAQUE_TEXTURE_REGISTER, grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
            layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{TRANSPARENCY_TEXTURE_REGISTER, grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
            PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mCompositionDescriptorSetLayout));
            PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mCompositionDescriptorSetLayout, &mCompositionDescriptorSet));

            grfx::WriteDescriptor writes[3] = {};

            writes[0].binding = COMPOSITION_SAMPLER_REGISTER;
            writes[0].type = grfx::DESCRIPTOR_TYPE_SAMPLER;
            writes[0].pSampler = mCompositionSampler;

            writes[1].binding = OPAQUE_TEXTURE_REGISTER;
            writes[1].arrayIndex = 0;
            writes[1].type = grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[1].pImageView = mOpaquePass->GetRenderTargetTexture(0)->GetSampledImageView();

            writes[2].binding = TRANSPARENCY_TEXTURE_REGISTER;
            writes[2].arrayIndex = 0;
            writes[2].type = grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[2].pImageView = mTransparencyTexture->GetSampledImageView();

            PPX_CHECKED_CALL(mCompositionDescriptorSet->UpdateDescriptors(3, writes));
        }

        // Pipeline
        {
            grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
            piCreateInfo.setCount                          = 1;
            piCreateInfo.sets[0].set                       = 0;
            piCreateInfo.sets[0].pLayout                   = mCompositionDescriptorSetLayout;
            PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mCompositionPipelineInterface));

            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Composition.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "Composition.ps", &PS));

            grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
            gpCreateInfo.VS                                 = {VS, "vsmain"};
            gpCreateInfo.PS                                 = {PS, "psmain"};
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
            gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
            gpCreateInfo.pPipelineInterface                 = mCompositionPipelineInterface;
            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mCompositionPipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }
    }
}

#if 0
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
    }

    // Pipeline
    {
        mAlphaBlending.pipelineInterface = mBackground.pipelineInterface;

        grfx::ShaderModulePtr VS, PS;
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlendingRender.vs", &VS));
        PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "AlphaBlendingRender.ps", &PS));

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
        piCreateInfo.setCount                          = 1;
        piCreateInfo.sets[0].set                       = 0;
        piCreateInfo.sets[0].pLayout                   = mAlphaBlending.descriptorSetLayout;
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
        createInfo.depthStencilUsageFlags       = grfx::IMAGE_USAGE_SAMPLED;
        createInfo.renderTargetInitialStates[0] = grfx::RESOURCE_STATE_SHADER_RESOURCE;
        createInfo.depthStencilInitialState     = grfx::RESOURCE_STATE_SHADER_RESOURCE;
        createInfo.renderTargetClearValues[0]   = {0, 0, 0, 0};
        PPX_CHECKED_CALL(GetDevice()->CreateDrawPass(&createInfo, &mMeshkin.transparencyPass));
    }

    // Descriptor
    {
        mMeshkin.renderDescriptorSetLayout = mBackground.descriptorSetLayout;
        mMeshkin.renderDescriptorSet = mBackground.descriptorSet;

        grfx::DescriptorSetLayoutCreateInfo layoutCreateInfo = {};
        layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{TRANSPARENCY_TEXTURE_REGISTER, grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
        layoutCreateInfo.bindings.push_back(grfx::DescriptorBinding{TRANSPARENCY_SAMPLER_REGISTER, grfx::DESCRIPTOR_TYPE_SAMPLER, 1, grfx::SHADER_STAGE_ALL_GRAPHICS});
        PPX_CHECKED_CALL(GetDevice()->CreateDescriptorSetLayout(&layoutCreateInfo, &mMeshkin.composeDescriptorSetLayout));

        PPX_CHECKED_CALL(GetDevice()->AllocateDescriptorSet(mDescriptorPool, mMeshkin.composeDescriptorSetLayout, &mMeshkin.composeDescriptorSet));

        grfx::WriteDescriptor write[2] = {};
        write[0].binding = TRANSPARENCY_TEXTURE_REGISTER;
        write[0].arrayIndex = 0;
        write[0].type = grfx::DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write[0].pImageView = mMeshkin.transparencyPass->GetRenderTargetTexture(0)->GetSampledImageView();

        write[1].binding = TRANSPARENCY_SAMPLER_REGISTER;
        write[1].type = grfx::DESCRIPTOR_TYPE_SAMPLER;
        write[1].pSampler = mCompositionSampler;
        PPX_CHECKED_CALL(mMeshkin.composeDescriptorSet->UpdateDescriptors(2, write));
    }

    // Pipeline
    {
        mMeshkin.renderPipelineInterface = mBackground.pipelineInterface;

        grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
        piCreateInfo.setCount                          = 1;
        piCreateInfo.sets[0].set                       = 0;
        piCreateInfo.sets[0].pLayout                   = mMeshkin.composeDescriptorSetLayout;
        PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mMeshkin.composePipelineInterface));

        {
            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "MeshkinRender.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "MeshkinRender.ps", &PS));

            grfx::GraphicsPipelineCreateInfo gpCreateInfo                        = {};
            gpCreateInfo.VS                                                      = {VS, "vsmain"};
            gpCreateInfo.PS                                                      = {PS, "psmain"};
            gpCreateInfo.vertexInputState.bindingCount                           = 1;
            gpCreateInfo.vertexInputState.bindings[0]                            = mMonkeyMesh->GetDerivedVertexBindings()[0];
            gpCreateInfo.inputAssemblyState.topology                             = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            gpCreateInfo.rasterState.polygonMode                                 = grfx::POLYGON_MODE_FILL;
            gpCreateInfo.rasterState.cullMode                                    = grfx::CULL_MODE_NONE;
            gpCreateInfo.rasterState.frontFace                                   = grfx::FRONT_FACE_CCW;
            gpCreateInfo.rasterState.rasterizationSamples                        = grfx::SAMPLE_COUNT_1;
            gpCreateInfo.depthStencilState.depthTestEnable                       = false;
            gpCreateInfo.depthStencilState.depthWriteEnable                      = false;
            gpCreateInfo.colorBlendState.blendAttachmentCount                    = 1;
            gpCreateInfo.colorBlendState.blendAttachments[0].blendEnable         = true;
            gpCreateInfo.colorBlendState.blendAttachments[0].srcColorBlendFactor = grfx::BLEND_FACTOR_ONE;
            gpCreateInfo.colorBlendState.blendAttachments[0].dstColorBlendFactor = grfx::BLEND_FACTOR_ONE;
            gpCreateInfo.colorBlendState.blendAttachments[0].colorBlendOp        = grfx::BLEND_OP_ADD;
            gpCreateInfo.colorBlendState.blendAttachments[0].srcAlphaBlendFactor = grfx::BLEND_FACTOR_ONE;
            gpCreateInfo.colorBlendState.blendAttachments[0].dstAlphaBlendFactor = grfx::BLEND_FACTOR_ONE;
            gpCreateInfo.colorBlendState.blendAttachments[0].alphaBlendOp        = grfx::BLEND_OP_ADD;
            gpCreateInfo.colorBlendState.blendAttachments[0].colorWriteMask      = grfx::ColorComponentFlags::RGBA();
            gpCreateInfo.outputState.renderTargetCount                           = 1;
            gpCreateInfo.outputState.renderTargetFormats[0]                      = mMeshkin.transparencyPass->GetRenderTargetTexture(0)->GetImageFormat();
            gpCreateInfo.outputState.depthStencilFormat                          = mMeshkin.transparencyPass->GetDepthStencilTexture()->GetImageFormat();
            gpCreateInfo.pPipelineInterface                                      = mMeshkin.renderPipelineInterface;

            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mMeshkin.renderPipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }

        {
            grfx::ShaderModulePtr VS, PS;
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "MeshkinCompose.vs", &VS));
            PPX_CHECKED_CALL(CreateShader("oit_demo/shaders", "MeshkinCompose.ps", &PS));

            grfx::PipelineInterfaceCreateInfo piCreateInfo = {};
            piCreateInfo.setCount                          = 1;
            piCreateInfo.sets[0].set                       = 0;
            piCreateInfo.sets[0].pLayout                   = mMeshkin.composeDescriptorSetLayout;
            PPX_CHECKED_CALL(GetDevice()->CreatePipelineInterface(&piCreateInfo, &mMeshkin.composePipelineInterface));

            grfx::GraphicsPipelineCreateInfo2 gpCreateInfo  = {};
            gpCreateInfo.VS                                 = {VS, "vsmain"};
            gpCreateInfo.PS                                 = {PS, "psmain"};
            gpCreateInfo.vertexInputState.bindingCount      = 0;
            gpCreateInfo.topology                           = grfx::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            gpCreateInfo.polygonMode                        = grfx::POLYGON_MODE_FILL;
            gpCreateInfo.cullMode                           = grfx::CULL_MODE_BACK;
            gpCreateInfo.frontFace                          = grfx::FRONT_FACE_CCW;
            gpCreateInfo.depthReadEnable                    = false;
            gpCreateInfo.depthWriteEnable                   = false;
            gpCreateInfo.blendModes[0]                      = grfx::BLEND_MODE_ALPHA;
            gpCreateInfo.outputState.renderTargetCount      = 1;
            gpCreateInfo.outputState.renderTargetFormats[0] = GetSwapchain()->GetColorFormat();
            gpCreateInfo.outputState.depthStencilFormat     = GetSwapchain()->GetDepthFormat();
            gpCreateInfo.pPipelineInterface                 = mMeshkin.composePipelineInterface;

            PPX_CHECKED_CALL(GetDevice()->CreateGraphicsPipeline(&gpCreateInfo, &mMeshkin.composePipeline));

            GetDevice()->DestroyShaderModule(VS);
            GetDevice()->DestroyShaderModule(PS);
        }
    }
}
#endif

void OITDemoApp::Setup()
{
    SetupCommon();
    //SetupBackground();
    //SetupAlphaBlending();
    //SetupMeshkin();
}

#if 0
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
#endif

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

#if 0
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

    // Render
    {
        mCommandBuffer->SetScissors(mMeshkin.transparencyPass->GetScissor());
        mCommandBuffer->SetViewports(mMeshkin.transparencyPass->GetViewport());

        mCommandBuffer->TransitionImageLayout(
            mMeshkin.transparencyPass,
            grfx::RESOURCE_STATE_SHADER_RESOURCE, grfx::RESOURCE_STATE_RENDER_TARGET,
            grfx::RESOURCE_STATE_SHADER_RESOURCE, grfx::RESOURCE_STATE_DEPTH_STENCIL_WRITE);
        mCommandBuffer->BeginRenderPass(mMeshkin.transparencyPass, grfx::DRAW_PASS_CLEAR_FLAG_CLEAR_RENDER_TARGETS);

        mCommandBuffer->BindGraphicsDescriptorSets(mMeshkin.renderPipelineInterface, 1, &mMeshkin.renderDescriptorSet);
        mCommandBuffer->BindGraphicsPipeline(mMeshkin.renderPipeline);
        mCommandBuffer->BindIndexBuffer(mMonkeyMesh);
        mCommandBuffer->BindVertexBuffers(mMonkeyMesh);
        mCommandBuffer->DrawIndexed(mMonkeyMesh->GetIndexCount());

        mCommandBuffer->EndRenderPass();
        mCommandBuffer->TransitionImageLayout(
            mMeshkin.transparencyPass,
            grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_SHADER_RESOURCE,
            grfx::RESOURCE_STATE_DEPTH_STENCIL_WRITE, grfx::RESOURCE_STATE_SHADER_RESOURCE);
    }

    // Compose
    {
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

        mCommandBuffer->BindGraphicsDescriptorSets(mMeshkin.composePipelineInterface, 1, &mMeshkin.composeDescriptorSet);
        mCommandBuffer->BindGraphicsPipeline(mMeshkin.composePipeline);
        mCommandBuffer->Draw(3, 1, 0, 0);

        DrawGui();

        mCommandBuffer->EndRenderPass();
        mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    }

    PPX_CHECKED_CALL(mCommandBuffer->End());
}
#endif

void OITDemoApp::Update()
{
    const float time = GetElapsedSeconds();

    // Shader globals
    {
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
}

void OITDemoApp::Render()
{
    uint32_t imageIndex = UINT32_MAX;
    PPX_CHECKED_CALL(GetSwapchain()->AcquireNextImage(UINT64_MAX, mImageAcquiredSemaphore, mImageAcquiredFence, &imageIndex));
    PPX_CHECKED_CALL(mImageAcquiredFence->WaitAndReset());
    PPX_CHECKED_CALL(mRenderCompleteFence->WaitAndReset());

    Update();

    // Record command buffer
#if 0
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
#else
    grfx::RenderPassPtr finalRenderPass = GetSwapchain()->GetRenderPass(imageIndex);
    PPX_ASSERT_MSG(!finalRenderPass.IsNull(), "render pass object is null");

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

    DrawGui();

    mCommandBuffer->EndRenderPass();
    mCommandBuffer->TransitionImageLayout(finalRenderPass->GetRenderTargetImage(0), PPX_ALL_SUBRESOURCES, grfx::RESOURCE_STATE_RENDER_TARGET, grfx::RESOURCE_STATE_PRESENT);
    PPX_CHECKED_CALL(mCommandBuffer->End());
#endif

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

