#include "Model.h"
#include <glm/mat4x2.hpp>
#include <glm/gtc/matrix_transform.inl>
#include <glm/gtc/type_ptr.inl>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "Utilities.h"

Model::Model(vks::VulkanDevice *vulkanDevice) : vulkanDevice(vulkanDevice), descriptorSet(0)
{
}

Model::~Model()
{
	destroy(*vulkanDevice);
}

void Model::updateUniformBuffer(glm::mat4 perspective, glm::vec3 rotation, float zoom, glm::vec2 translate)
{
	uboVS.projection = perspective;

	glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(translate, zoom));

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

	if (vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set");
	}

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
}

void Model::loadModel(std::string filename, VkQueue queue, VkDevice device)
{
	// Load the model from file using ASSIMP

	const aiScene* scene;
	Assimp::Importer Importer;

	// Flags for loading the mesh
	static const int assimpFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices;

	scene = Importer.ReadFile(filename.c_str(), assimpFlags);

	// Generate vertex buffer from ASSIMP scene data
	float scale = 1.0f;
	std::vector<Vertex> vertexBuffer;

	// Iterate through all meshes in the file and extract the vertex components
	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
	{
		for (uint32_t v = 0; v < scene->mMeshes[m]->mNumVertices; v++)
		{
			Vertex vertex;

			// Use glm make_* functions to convert ASSIMP vectors to glm vectors
			vertex.pos = glm::make_vec3(&scene->mMeshes[m]->mVertices[v].x) * scale;
			vertex.normal = glm::make_vec3(&scene->mMeshes[m]->mNormals[v].x);
			// Texture coordinates and colors may have multiple channels, we only use the first [0] one
			vertex.uv = glm::make_vec2(&scene->mMeshes[m]->mTextureCoords[0][v].x);
			// Mesh may not have vertex colors
			vertex.color = (scene->mMeshes[m]->HasVertexColors(0)) ? glm::make_vec3(&scene->mMeshes[m]->mColors[0][v].r) : glm::vec3(1.0f);

			// Vulkan uses a right-handed NDC (contrary to OpenGL), so simply flip Y-Axis
			vertex.pos.y *= -1.0f;

			vertexBuffer.push_back(vertex);
		}
	}
	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);

	// Generate index buffer from ASSIMP scene data
	std::vector<uint32_t> indexBuffer;
	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
	{
		uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
		for (uint32_t f = 0; f < scene->mMeshes[m]->mNumFaces; f++)
		{
			// We assume that all faces are triangulated
			for (uint32_t i = 0; i < 3; i++)
			{
				indexBuffer.push_back(scene->mMeshes[m]->mFaces[f].mIndices[i] + indexBase);
			}
		}
	}
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);

	this->indices.count = static_cast<uint32_t>(indexBuffer.size());

	// Static mesh should always be device local
	bool useStaging = true;

	if (useStaging)
	{
		struct {
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging, indexStaging;

		// Create staging buffers
		// Vertex data
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBufferSize,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertexBuffer.data()));
		// Index data
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			indexBufferSize,
			&indexStaging.buffer,
			&indexStaging.memory,
			indexBuffer.data()));

		// Create device local buffers
		// Vertex buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize,
			&this->vertices.buffer,
			&this->vertices.memory));
		// Index buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&this->indices.buffer,
			&this->indices.memory));

		// Copy from staging buffers
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			vertexStaging.buffer,
			this->vertices.buffer,
			1,
			&copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			indexStaging.buffer,
			this->indices.buffer,
			1,
			&copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
		vkFreeMemory(device, vertexStaging.memory, nullptr);
		vkDestroyBuffer(device, indexStaging.buffer, nullptr);
		vkFreeMemory(device, indexStaging.memory, nullptr);
	}
	else
	{
		// Vertex buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			vertexBufferSize,
			&this->vertices.buffer,
			&this->vertices.memory,
			vertexBuffer.data()));
		// Index buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			indexBufferSize,
			&this->indices.buffer,
			&this->indices.memory,
			indexBuffer.data()));
	}

	this->prepareUniformBuffers();
}

std::string Model::getId()
{
	return id;
}

void Model::setId(std::string id)
{
	this->id = id;
}
