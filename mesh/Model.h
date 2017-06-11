#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"

// Contains all Vulkan resources required to represent vertex and index buffers for a model
// This is for demonstration and learning purposes, the other examples use a model loader class for easy access
class Model
{
public:
	struct
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertices;

	struct
	{
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

	struct
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(25.0f, 5.0f, 5.0f, 1.0f);
	} uboVS;

	struct
	{
		vks::Buffer scene;
	} uniformBuffers;

	struct
	{
		vks::Texture2D colorMap;
	} textures;

	vks::VulkanDevice *vulkanDevice;
	VkDescriptorSet descriptorSet;

	Model(vks::VulkanDevice *vulkanDevice);
	~Model();

	void updateUniformBuffer(glm::mat4 perspective, glm::vec3 rotation, float zoom);

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers();

	void setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout);

	// Destroys all Vulkan resources created for this model
	void destroy(VkDevice device);
};


