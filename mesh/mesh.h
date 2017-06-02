#pragma once

/*
* Vulkan Example - Model loading and rendering
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"

#include "Utilities.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	bool wireframe = false;

	struct
	{
		vks::Texture2D colorMap;
	} textures;

	struct
	{
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct
	{
		vks::Buffer scene;
	} uniformBuffers;

	struct
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(25.0f, 5.0f, 5.0f, 1.0f);
	} uboVS;

	std::vector<Model*> models;
	Model model;
	Pipelines pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	VulkanExample();
	~VulkanExample();
	

	virtual void getEnabledFeatures();

	void reBuildCommandBuffers();

	void buildCommandBuffers();

	// Load a model from file using the ASSIMP model loader and generate all resources required to render the model
	void loadModel(std::string filename);

	void loadAssets();

	void setupVertexDescriptions();

	void setupDescriptorPool();

	void setupDescriptorSetLayout();

	void setupDescriptorSet();

	void preparePipelines();

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
		VK_CHECK_RESULT(uniformBuffers.scene.map());

		updateUniformBuffers();
	}

	void updateUniformBuffers();

	void draw();

	void prepare();

	virtual void render();

	virtual void viewChanged();

	virtual void keyPressed(uint32_t keyCode);

	virtual void getOverlayText(VulkanTextOverlay *textOverlay);
};