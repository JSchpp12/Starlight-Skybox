#include "star_skybox/context/SkyboxContext.hpp"

#include <starlight/command/pipeline/CreatePipeline.hpp>
#include <starlight/command/shader/CreateShader.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/device/managers/DescriptorPool.hpp>
#include <starlight/enums/Enums.hpp>
#include <starlight/wrappers/graphics/StarDescriptorBuilders.hpp>

#include <cassert>
#include <filesystem>

namespace star::skybox
{
static vk::PushConstantRange ConvertPushConstantRange()
{
    return vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setOffset(0).setSize(16u);
}

SkyboxContext::SkyboxContext(star::core::device::DeviceContext &deviceContext) : m_deviceContext(deviceContext)
{
}

SkyboxContext::~SkyboxContext()
{
    // Documented ordering rule: this must run while the device is alive.
    vk::Device device = m_deviceContext.getDevice().getVulkanDevice();

    if (m_pipeline && m_pipeline->pipelineLayout)
        device.destroyPipelineLayout(m_pipeline->pipelineLayout);

    if (m_setLayout)
        m_setLayout->cleanupRender(m_deviceContext.getDevice());
}

void SkyboxContext::ensureConversionResources()
{
    if (!m_pool)
    {
        auto &poolManager = m_deviceContext.getDescriptorPoolManager();

        // createRecord() builds the pool synchronously (no GPU work), so the handle is ready immediately. The manager
        // owns it until device teardown; the context keeps only the raw handle for the per-make requests.
        const star::Handle poolHandle =
            poolManager.submit(star::core::device::manager::DescriptorPoolRequest{
                .requests = {{vk::DescriptorType::eStorageBuffer, ConversionMaxSets},
                             {vk::DescriptorType::eStorageImage, ConversionMaxSets}},
                .poolFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = ConversionMaxSets});

        auto *record = poolManager.get(poolHandle);
        assert(record != nullptr && record->pool != nullptr &&
               "The conversion descriptor pool was not created synchronously");
        m_pool = record->pool->getDescriptorPool();
    }

    if (m_pipeline)
        return;

    auto &cmdBus = m_deviceContext.getCmdBus();
    vk::Device device = m_deviceContext.getDevice().getVulkanDevice();

    m_setLayout = star::StarDescriptorSetLayout::Builder()
                      .addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
                      .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
                      .build(m_deviceContext.getDevice());

    const vk::DescriptorSetLayout setLayout = m_setLayout->getDescriptorSetLayout();
    const vk::PushConstantRange pushConstantRange = ConvertPushConstantRange();
    const vk::PipelineLayoutCreateInfo pipelineLayoutInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(setLayout).setPushConstantRanges(pushConstantRange);
    const vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

    star::command::shader::CreateShader shaderCmd;
    shaderCmd.setStage(star::Shader_Stage::compute)
        .setPath(std::filesystem::path{"shaders"} / "skybox" / "equirect2cube.comp");
    cmdBus.submit(shaderCmd);
    const star::Handle shader = shaderCmd.getReply().get();

    star::command::pipeline::CreatePipeline pipelineCmd;
    pipelineCmd.setComputePipeline().setPipelineLayout(pipelineLayout).addShaderHandle(shader);
    cmdBus.submit(pipelineCmd);
    const star::Handle pipeline = pipelineCmd.getReply().get();

    m_pipeline = SkyboxConversionPipeline{
        .pipeline = pipeline, .descriptorSetLayout = setLayout, .pipelineLayout = pipelineLayout};
}

const SkyboxConversionPipeline &SkyboxContext::conversionPipeline() const
{
    assert(m_pipeline.has_value() && "ensureConversionResources() must run before conversionPipeline()");
    return *m_pipeline;
}

vk::DescriptorPool SkyboxContext::conversionPool() const
{
    assert(m_pool && "ensureConversionResources() must run before conversionPool()");
    return m_pool;
}
} // namespace star::skybox
