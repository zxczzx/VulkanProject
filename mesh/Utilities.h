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

struct Pipelines
{
	VkPipeline solid;
	VkPipeline wireframe = VK_NULL_HANDLE;
};