#include <edbr/Graphics/Pipelines/SkyboxPipeline.h>

#include <volk.h>

#include <edbr/Graphics/Camera.h>
#include <edbr/Graphics/GfxDevice.h>

#include <edbr/Graphics/Vulkan/Pipelines.h>
#include <edbr/Graphics/Vulkan/Util.h>

void SkyboxPipeline::init(
    GfxDevice& gfxDevice,
    VkFormat drawImageFormat,
    VkFormat depthImageFormat,
    VkSampleCountFlagBits samples)
{
    const auto& device = gfxDevice.getDevice();

    const auto vertexShader =
        vkutil::loadShaderModule("shaders/fullscreen_triangle.vert.spv", device);
    const auto fragShader = vkutil::loadShaderModule("shaders/skybox.frag.spv", device);

    const auto bufferRange = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(SkyboxPushConstants),
    };

    const auto pushConstantRanges = std::array{bufferRange};
    const auto layouts = std::array{gfxDevice.getBindlessDescSetLayout()};
    pipelineLayout = vkutil::createPipelineLayout(device, layouts, pushConstantRanges);

    pipeline = PipelineBuilder{pipelineLayout}
                   .setShaders(vertexShader, fragShader)
                   .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                   .setPolygonMode(VK_POLYGON_MODE_FILL)
                   .disableCulling()
                   .setMultisampling(samples)
                   .disableBlending()
                   .setColorAttachmentFormat(drawImageFormat)
                   .setDepthFormat(depthImageFormat)
                   // only draw to fragments with depth == 0.0 only
                   .enableDepthTest(false, VK_COMPARE_OP_EQUAL)
                   .build(device);
    vkutil::addDebugLabel(device, pipeline, "skybox pipeline");

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void SkyboxPipeline::cleanup(VkDevice device)
{
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

void SkyboxPipeline::setSkyboxImage(const ImageId skyboxId)
{
    skyboxTextureId = skyboxId;
}

void SkyboxPipeline::draw(VkCommandBuffer cmd, GfxDevice& gfxDevice, const Camera& camera)
{
    if (skyboxTextureId == NULL_IMAGE_ID) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    gfxDevice.bindBindlessDescSet(cmd, pipelineLayout);

    const auto pcs = SkyboxPushConstants{
        .invViewProj = glm::inverse(camera.getViewProj()),
        .cameraPos = glm::vec4{camera.getPosition(), 1.f},
        .skyboxTextureId = (std::uint32_t)skyboxTextureId,
    };
    vkCmdPushConstants(
        cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxPushConstants), &pcs);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}
