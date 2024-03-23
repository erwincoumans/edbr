#include <edbr/Graphics/Pipelines/CSMPipeline.h>

#include <edbr/Graphics/Vulkan/Init.h>
#include <edbr/Graphics/Vulkan/Pipelines.h>
#include <edbr/Graphics/Vulkan/Util.h>

#include <edbr/Graphics/BaseRenderer.h>
#include <edbr/Graphics/FrustumCulling.h>
#include <edbr/Graphics/GfxDevice.h>
#include <edbr/Graphics/MeshDrawCommand.h>
#include <edbr/Graphics/ShadowMapping.h>

void CSMPipeline::init(GfxDevice& gfxDevice, const std::array<float, NUM_SHADOW_CASCADES>& percents)
{
    this->percents = percents;
    const auto& device = gfxDevice.getDevice();
    const auto vertexShader = vkutil::loadShaderModule("shaders/mesh_depth_only.vert.spv", device);

    vkutil::addDebugLabel(device, vertexShader, "mesh_depth_only.vert");

    const auto bufferRange = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };

    const auto pushConstantRanges = std::array{bufferRange};
    pipelineLayout = vkutil::createPipelineLayout(device, {}, pushConstantRanges);

    pipeline = PipelineBuilder{pipelineLayout}
                   .setShaders(vertexShader, VK_NULL_HANDLE)
                   .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                   .setPolygonMode(VK_POLYGON_MODE_FILL)
                   .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                   .setMultisamplingNone()
                   .disableBlending()
                   .setDepthFormat(VK_FORMAT_D32_SFLOAT)
                   .enableDepthClamp()
                   .enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                   .build(device);
    vkutil::addDebugLabel(device, pipeline, "mesh depth only pipeline");

    vkDestroyShaderModule(device, vertexShader, nullptr);

    initCSMData(gfxDevice);
}

void CSMPipeline::initCSMData(GfxDevice& gfxDevice)
{
    csmShadowMapID = gfxDevice.createImage(
        {
            .format = VK_FORMAT_D32_SFLOAT,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .extent =
                VkExtent3D{
                    (std::uint32_t)shadowMapTextureSize, (std::uint32_t)shadowMapTextureSize, 1},
            .numLayers = NUM_SHADOW_CASCADES,
        },
        "CSM shadow map");
    const auto& csmShadowMap = gfxDevice.getImage(csmShadowMapID);

    for (int i = 0; i < NUM_SHADOW_CASCADES; ++i) {
        const auto createInfo = VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = csmShadowMap.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = csmShadowMap.format,
            .subresourceRange =
                VkImageSubresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = (std::uint32_t)i,
                    .layerCount = 1,
                },
        };
        VK_CHECK(
            vkCreateImageView(gfxDevice.getDevice(), &createInfo, nullptr, &csmShadowMapViews[i]));
        vkutil::addDebugLabel(gfxDevice.getDevice(), csmShadowMapViews[i], "CSM shadow map view");
    }
}

void CSMPipeline::cleanup(GfxDevice& gfxDevice)
{
    vkDestroyPipelineLayout(gfxDevice.getDevice(), pipelineLayout, nullptr);
    vkDestroyPipeline(gfxDevice.getDevice(), pipeline, nullptr);
    for (int i = 0; i < NUM_SHADOW_CASCADES; ++i) {
        vkDestroyImageView(gfxDevice.getDevice(), csmShadowMapViews[i], nullptr);
    }
}

void CSMPipeline::draw(
    VkCommandBuffer cmd,
    const GfxDevice& gfxDevice,
    const BaseRenderer& renderer,
    const Camera& camera,
    const glm::vec3& sunlightDirection,
    const std::vector<MeshDrawCommand>& meshDrawCommands,
    bool shadowsEnabled)
{
    const auto& csmShadowMap = gfxDevice.getImage(csmShadowMapID);

    vkutil::transitionImage(
        cmd,
        csmShadowMap.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    for (std::size_t i = 0; i < NUM_SHADOW_CASCADES; ++i) {
        float zNear = i == 0 ? camera.getZNear() : camera.getZNear() * percents[i - 1];
        float zFar = camera.getZFar() * percents[i];
        cascadeFarPlaneZs[i] = zFar;

        // create subfustrum by copying everything about the main camera,
        // but changing zFar
        Camera subFrustumCamera;
        subFrustumCamera.setPosition(camera.getPosition());
        subFrustumCamera.setHeading(camera.getHeading());
        subFrustumCamera.init(camera.getFOVX(), zNear, zFar, 1.f);

        const auto corners = edge::calculateFrustumCornersWorldSpace(subFrustumCamera);
        const auto csmCamera = calculateCSMCamera(corners, sunlightDirection, shadowMapTextureSize);
        csmLightSpaceTMs[i] = csmCamera.getViewProj();

        const auto renderInfo = vkutil::createRenderingInfo({
            .renderExtent =
                {(std::uint32_t)shadowMapTextureSize, (std::uint32_t)shadowMapTextureSize},
            .depthImageView = csmShadowMapViews[i],
            .depthImageClearValue = 0.f,
        });
        vkCmdBeginRendering(cmd, &renderInfo.renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        const auto viewport = VkViewport{
            .x = 0,
            .y = 0,
            .width = shadowMapTextureSize,
            .height = shadowMapTextureSize,
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        const auto scissor = VkRect2D{
            .offset = {},
            .extent = {(std::uint32_t)shadowMapTextureSize, (std::uint32_t)shadowMapTextureSize},
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        const auto frustum = edge::createFrustumFromCamera(csmCamera);
        auto prevMeshId = NULL_MESH_ID;

        for (const auto& dc : meshDrawCommands) {
            if (!shadowsEnabled || !dc.castShadow) {
                continue;
            }

            if (!edge::isInFrustum(frustum, dc.worldBoundingSphere)) {
                // hack: don't cull big objects, because shadows from them might disappear
                if (dc.worldBoundingSphere.radius < 2.f) {
                    continue;
                }
            }

            const auto& mesh = renderer.getMesh(dc.meshId);

            if (dc.meshId != prevMeshId) {
                prevMeshId = dc.meshId;
                vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            }

            const auto pushConstants = PushConstants{
                .mvp = csmLightSpaceTMs[i] * dc.transformMatrix,
                .vertexBuffer = dc.skinnedMesh ? dc.skinnedMesh->skinnedVertexBuffer.address :
                                                 mesh.vertexBuffer.address,
            };
            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstants),
                &pushConstants);

            vkCmdDrawIndexed(cmd, mesh.numIndices, 1, 0, 0, 0);
        }

        vkCmdEndRendering(cmd);
    }

    // this also gives us sync with future passes that will read from CSM shadow map
    vkutil::transitionImage(
        cmd,
        csmShadowMap.image,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
}