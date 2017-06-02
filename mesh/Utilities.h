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

	// Destroys all Vulkan resources created for this model
	void destroy(VkDevice device)
	{
		vkDestroyBuffer(device, vertices.buffer, nullptr);
		vkFreeMemory(device, vertices.memory, nullptr);
		vkDestroyBuffer(device, indices.buffer, nullptr);
		vkFreeMemory(device, indices.memory, nullptr);
	};
};

struct Pipelines
{
	VkPipeline solid;
	VkPipeline wireframe = VK_NULL_HANDLE;
};