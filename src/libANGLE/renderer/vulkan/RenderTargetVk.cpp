//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#include "libANGLE/renderer/vulkan/RenderTargetVk.h"

#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
RenderTargetVk::RenderTargetVk(vk::ImageHelper *image,
                               vk::ImageView *imageView,
                               vk::CommandGraphResource *resource)
    : mImage(image), mImageView(imageView), mResource(resource)
{
}

RenderTargetVk::~RenderTargetVk()
{
}

RenderTargetVk::RenderTargetVk(RenderTargetVk &&other)
    : mImage(other.mImage), mImageView(other.mImageView), mResource(other.mResource)
{
}

void RenderTargetVk::onColorDraw(vk::CommandGraphResource *framebufferVk,
                                 vk::CommandBuffer *commandBuffer,
                                 vk::RenderPassDesc *renderPassDesc)
{
    ASSERT(commandBuffer->valid());
    ASSERT(!mImage->getFormat().textureFormat().hasDepthOrStencilBits());

    // Store the attachment info in the renderPassDesc.
    renderPassDesc->packColorAttachment(*mImage);

    // TODO(jmadill): Use automatic layout transition. http://anglebug.com/2361
    mImage->changeLayoutWithStages(VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, commandBuffer);

    // Set up dependencies between the RT resource and the Framebuffer.
    mResource->addWriteDependency(framebufferVk);
}

void RenderTargetVk::onDepthStencilDraw(vk::CommandGraphResource *framebufferVk,
                                        vk::CommandBuffer *commandBuffer,
                                        vk::RenderPassDesc *renderPassDesc)
{
    ASSERT(commandBuffer->valid());
    ASSERT(mImage->getFormat().textureFormat().hasDepthOrStencilBits());

    // Store the attachment info in the renderPassDesc.
    renderPassDesc->packDepthStencilAttachment(*mImage);

    // TODO(jmadill): Use automatic layout transition. http://anglebug.com/2361
    const angle::Format &format    = mImage->getFormat().textureFormat();
    VkImageAspectFlags aspectFlags = vk::GetDepthStencilAspectFlags(format);

    mImage->changeLayoutWithStages(aspectFlags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, commandBuffer);

    // Set up dependencies between the RT resource and the Framebuffer.
    mResource->addWriteDependency(framebufferVk);
}

const vk::ImageHelper &RenderTargetVk::getImage() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

vk::ImageView *RenderTargetVk::getImageView() const
{
    ASSERT(mImageView && mImageView->valid());
    return mImageView;
}

vk::CommandGraphResource *RenderTargetVk::getResource() const
{
    return mResource;
}

const vk::Format &RenderTargetVk::getImageFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getFormat();
}

const gl::Extents &RenderTargetVk::getImageExtents() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getExtents();
}

void RenderTargetVk::updateSwapchainImage(vk::ImageHelper *image, vk::ImageView *imageView)
{
    ASSERT(image && image->valid() && imageView && imageView->valid());
    mImage     = image;
    mImageView = imageView;
}

vk::ImageHelper *RenderTargetVk::getImageForRead(vk::CommandGraphResource *readingResource,
                                                 VkImageLayout layout,
                                                 vk::CommandBuffer *commandBuffer)
{
    ASSERT(mImage && mImage->valid());

    // TODO(jmadill): Better simultaneous resource access. http://anglebug.com/2679
    mResource->addWriteDependency(readingResource);

    mImage->changeLayoutWithStages(mImage->getAspectFlags(), layout,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, commandBuffer);

    return mImage;
}

vk::ImageHelper *RenderTargetVk::getImageForWrite(vk::CommandGraphResource *writingResource) const
{
    ASSERT(mImage && mImage->valid());
    mResource->addWriteDependency(writingResource);
    return mImage;
}

}  // namespace rx
