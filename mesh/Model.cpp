#include "Model.h"
#include <glm/mat4x2.hpp>
#include <glm/gtc/matrix_transform.inl>

Model::Model(vks::VulkanDevice *vulkanDevice) : vulkanDevice(vulkanDevice), descriptorSet(0)
{
}

Model::~Model()
{
	destroy(*vulkanDevice);
}

void Model::updateUniformBuffer(glm::mat4 perspective, glm::vec3 rotation, float zoom)
{
	uboVS.projection = perspective;

	glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

	uboVS.model = viewMatrix * glm::translate(glm::mat4(), { 0.1f, 1.1f, 0.0f });
	uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	memcpy(uniformBuffers.scene.mapped, &uboVS, sizeof(uboVS));
}

void Model::prepareUniformBuffers()
{
	// Vertex shader uniform buffer block
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&uniformBuffers.scene,
		sizeof(uboVS)));

	// Map persistent
	VK_CHECK_RESULT(uniformBuffers.scene.map())
}

void Model::setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout)
{
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(
			pool,
			&descriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet));

	VkDescriptorImageInfo texDescriptor =
		vks::initializers::descriptorImageInfo(
			textures.colorMap.sampler,
			textures.colorMap.view,
			VK_IMAGE_LAYOUT_GENERAL);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
	{
		// Binding 0 : Vertex shader uniform buffer
		vks::initializers::writeDescriptorSet(
			descriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&uniformBuffers.scene.descriptor),
		// Binding 1 : Color map 
		vks::initializers::writeDescriptorSet(
			descriptorSet,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&texDescriptor)
	};

	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void Model::destroy(VkDevice device)
{
	vkDestroyBuffer(device, vertices.buffer, nullptr);
	vkFreeMemory(device, vertices.memory, nullptr);
	vkDestroyBuffer(device, indices.buffer, nullptr);
	vkFreeMemory(device, indices.memory, nullptr);
	uniformBuffers.scene.destroy();
	textures.colorMap.destroy();
};