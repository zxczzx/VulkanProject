#pragma once

// Vertex layout used in this example
// This must fit input locations of the vertex shader used to render the model
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
	glm::vec3 color;
};

// Contains all Vulkan resources required to represent vertex and index buffers for a model
// This is for demonstration and learning purposes, the other examples use a model loader class for easy access
struct Model
{
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

	Model(vks::VulkanDevice *vulkanDevice) : vulkanDevice(vulkanDevice), descriptorSet(0)
	{
	};
	~Model()
	{
		destroy(*vulkanDevice);
	}

	void updateUniformBuffer(glm::mat4 perspective, glm::vec3 rotation, float zoom)
	{
		uboVS.projection = perspective;

		glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.model = viewMatrix * glm::translate(glm::mat4(), { 0.1f, 1.1f, 0.0f });
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffers.scene.mapped, &uboVS, sizeof(uboVS));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
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

	void setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout)
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

	// Destroys all Vulkan resources created for this model
	void destroy(VkDevice device)
	{
		vkDestroyBuffer(device, vertices.buffer, nullptr);
		vkFreeMemory(device, vertices.memory, nullptr);
		vkDestroyBuffer(device, indices.buffer, nullptr);
		vkFreeMemory(device, indices.memory, nullptr);
		uniformBuffers.scene.destroy();
		textures.colorMap.destroy();
	};
};

struct Pipelines
{
	VkPipeline solid;
	VkPipeline wireframe = VK_NULL_HANDLE;
};