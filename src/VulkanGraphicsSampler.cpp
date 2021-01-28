#include "VulkanGraphicsSampler.h"

#include <stdexcept>

vgfx::Sampler::Sampler(
    Context& context,
    const Config& config)
    : m_context(context)
{
    VkResult result =
        vkCreateSampler(
            m_context.getLogicalDevice(),
            &config.samplerInfo,
            m_context.getAllocationCallbacks(),
            &m_sampler);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }
}

vgfx::Sampler::~Sampler()
{
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(
            m_context.getLogicalDevice(),
            m_sampler,
            m_context.getAllocationCallbacks());
    }
}
