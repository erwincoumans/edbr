#include "Cubemap.h"

#include <Graphics/Renderer.h>
#include <Graphics/Vulkan/Util.h>

#include <util/ImageLoader.h>

namespace graphics
{
AllocatedImage loadCubemap(const Renderer& renderer, const std::filesystem::path& imagesDir)
{
    VkImageUsageFlags usages{};
    usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    usages |= VK_IMAGE_USAGE_STORAGE_BIT;
    usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    AllocatedImage img;

    static const auto paths =
        std::array{"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"};

    std::uint32_t face = 0;
    bool imageCreated = false;

    for (auto& p : paths) {
        ImageData data = util::loadImage(imagesDir / p);
        assert(data.channels == 4);
        assert(data.pixels != nullptr);

        if (!imageCreated) {
            img = renderer.createImage({
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .extent =
                    {
                        .width = (std::uint32_t)data.width,
                        .height = (std::uint32_t)data.height,
                        .depth = 1,
                    },
                .numLayers = 6,
                .isCubemap = true,
            });
            imageCreated = true;
        } else {
            assert(
                img.extent.width == (std::uint32_t)data.width &&
                img.extent.height == (std::uint32_t)data.height &&
                "All images for cubemap must have the same size");
        }

        renderer.uploadImageData(img, data.pixels, face);
        ++face;
    }

    const auto cubemapLabel = "cubemap, dir=" + imagesDir.string();
    vkutil::addDebugLabel(renderer.getDevice(), img.image, cubemapLabel.c_str());

    return img;
}
}