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

#include "shaders/Common.hlsli"

class XlsViewerApp
    : public ppx::Application
{
public:
    virtual void Config(ppx::ApplicationSettings& settings) override;
    virtual void Setup() override;
    virtual void Render() override;

private:
	enum
	{
		BYTES_PER_PIXEL = sizeof(Pixel),
		GRAPHICS_BUFFER_SIZE = 4096 * 4096 * BYTES_PER_PIXEL,
	};

private:
    void RecordCommandBuffer(uint32_t imageIndex);

private:
    ppx::grfx::SemaphorePtr         mImageAcquiredSemaphore;
    ppx::grfx::FencePtr             mImageAcquiredFence;
    ppx::grfx::SemaphorePtr         mRenderCompleteSemaphore;
    ppx::grfx::FencePtr             mRenderCompleteFence;

    ppx::grfx::CommandBufferPtr     mCommandBuffer;

    ppx::grfx::BufferPtr            mGraphicsBuffer;
    ppx::grfx::PipelineInterfacePtr mGraphicsPipelineInterface;
    ppx::grfx::GraphicsPipelinePtr  mGraphicsPipeline;
};

