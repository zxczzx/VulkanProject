#include "mesh.h"

VulkanExample::VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
{
	zoom = -5.5f;
	zoomSpeed = 2.5f;
	rotationSpeed = 0.5f;
	rotation = { -0.5f, -112.75f, 0.0f };
	cameraPos = { 0.1f, 1.1f, 0.0f };
	enableTextOverlay = true;
	title = "Vulkan Example - Model rendering";

}

VulkanExample::~VulkanExample()
{
	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(device, pipelines.solid, nullptr);
	if (pipelines.wireframe != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
	}

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	for (auto& model : models)
	{
		model->destroy(device);
	}
}

void VulkanExample::getEnabledFeatures()
{
	// Fill mode non solid is required for wireframe display
	if (deviceFeatures.fillModeNonSolid)
	{
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	};
}

void VulkanExample::reBuildCommandBuffers()
{
	if (!checkCommandBuffers())
	{
		destroyCommandBuffers();
		createCommandBuffers();
	}
	buildCommandBuffers();
}

void VulkanExample::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);

		for (auto model : models)
		{
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &model->descriptorSet, 0, NULL);
			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(drawCmdBuffers[i], model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(drawCmdBuffers[i], model->indices.count, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(drawCmdBuffers[i]);
		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanExample::loadModel(std::string filename, Model& model)
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

	model.indices.count = static_cast<uint32_t>(indexBuffer.size());

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
			&model.vertices.buffer,
			&model.vertices.memory));
		// Index buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&model.indices.buffer,
			&model.indices.memory));

		// Copy from staging buffers
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			vertexStaging.buffer,
			model.vertices.buffer,
			1,
			&copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			indexStaging.buffer,
			model.indices.buffer,
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
			&model.vertices.buffer,
			&model.vertices.memory,
			vertexBuffer.data()));
		// Index buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			indexBufferSize,
			&model.indices.buffer,
			&model.indices.memory,
			indexBuffer.data()));
	}

	model.prepareUniformBuffers();
}

void VulkanExample::loadAssets()
{
	const int MODELS_COUNT = 1;
	models.resize(MODELS_COUNT);

	for (int i = 0; i < MODELS_COUNT; i++)
	{
		models[i] = new Model(vulkanDevice);
		loadModel(getAssetPath() + "models/voyager/voyager.dae", *models[i]);
		if (deviceFeatures.textureCompressionBC) 
		{
			models[i]->textures.colorMap.loadFromFile(getAssetPath() + "models/voyager/voyager_bc3_unorm.ktx", VK_FORMAT_BC3_UNORM_BLOCK, vulkanDevice, queue);
		}
		else if (deviceFeatures.textureCompressionASTC_LDR) 
		{
			models[i]->textures.colorMap.loadFromFile(getAssetPath() + "models/voyager/voyager_astc_8x8_unorm.ktx", VK_FORMAT_ASTC_8x8_UNORM_BLOCK, vulkanDevice, queue);
		}
		else if (deviceFeatures.textureCompressionETC2) 
		{
			models[i]->textures.colorMap.loadFromFile(getAssetPath() + "models/voyager/voyager_etc2_unorm.ktx", VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, vulkanDevice, queue);
		}
		else 
		{
			vks::tools::exitFatal("Device does not support any compressed texture format!", "Error");
		}
	}
}

void VulkanExample::setupVertexDescriptions()
{
	// Binding description
	vertices.bindingDescriptions.resize(1);
	vertices.bindingDescriptions[0] =
		vks::initializers::vertexInputBindingDescription(
			VERTEX_BUFFER_BIND_ID,
			sizeof(Vertex),
			VK_VERTEX_INPUT_RATE_VERTEX);

	// Attribute descriptions
	// Describes memory layout and shader positions
	vertices.attributeDescriptions.resize(4);
	// Location 0 : Position
	vertices.attributeDescriptions[0] =
		vks::initializers::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, pos));
	// Location 1 : Normal
	vertices.attributeDescriptions[1] =
		vks::initializers::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, normal));
	// Location 2 : Texture coordinates
	vertices.attributeDescriptions[2] =
		vks::initializers::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			2,
			VK_FORMAT_R32G32_SFLOAT,
			offsetof(Vertex, uv));
	// Location 3 : Color
	vertices.attributeDescriptions[3] =
		vks::initializers::vertexInputAttributeDescription(
			VERTEX_BUFFER_BIND_ID,
			3,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, color));

	vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
	vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void VulkanExample::setupDescriptorPool()
{
	// Example uses one ubo and one combined image sampler
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vks::initializers::descriptorPoolCreateInfo(
			static_cast<uint32_t>(poolSizes.size()),
			poolSizes.data(),
			1);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VulkanExample::setupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0),
		// Binding 1 : Fragment shader combined sampler
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1),
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vks::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vks::initializers::pipelineLayoutCreateInfo(
			&descriptorSetLayout,
			1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void VulkanExample::setupDescriptorSet()
{
	for (auto& model : models)
	{
		model->setupDescriptorSet(descriptorPool, descriptorSetLayout);
	}
}

void VulkanExample::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vks::initializers::pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vks::initializers::pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE,
			0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vks::initializers::pipelineColorBlendAttachmentState(
			0xf,
			VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vks::initializers::pipelineColorBlendStateCreateInfo(
			1,
			&blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vks::initializers::pipelineDepthStencilStateCreateInfo(
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
		vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
		vks::initializers::pipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT,
			0);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		vks::initializers::pipelineDynamicStateCreateInfo(
			dynamicStateEnables.data(),
			static_cast<uint32_t>(dynamicStateEnables.size()),
			0);

	// Solid rendering pipeline
	// Load shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0] = loadShader(getAssetPath() + "shaders/mesh/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/mesh/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vks::initializers::pipelineCreateInfo(
			pipelineLayout,
			renderPass,
			0);

	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solid));

	// Wire frame rendering pipeline
	if (deviceFeatures.fillModeNonSolid) {
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationState.lineWidth = 1.0f;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
	}
}

void VulkanExample::updateUniformBuffers()
{
	for (auto model : models)
	{
		glm::mat4 perspective = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
		model->updateUniformBuffer(perspective, rotation, zoom);
	}
}

void VulkanExample::draw()
{
	VulkanExampleBase::prepareFrame();

	// Command buffer to be sumitted to the queue
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

	// Submit to queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

	VulkanExampleBase::submitFrame();
}

void VulkanExample::prepare()
{
	VulkanExampleBase::prepare();
	loadAssets();
	setupVertexDescriptions();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	updateUniformBuffers();
	buildCommandBuffers();
	prepared = true;
}

void VulkanExample::render()
{
	if (!prepared)
		return;
	vkDeviceWaitIdle(device);
	draw();
	vkDeviceWaitIdle(device);
	if (!paused)
	{
		updateUniformBuffers();
	}
}

void VulkanExample::viewChanged()
{
	updateUniformBuffers();
}

void VulkanExample::keyPressed(uint32_t keyCode)
{
	switch (keyCode)
	{
	case KEY_W:
	case GAMEPAD_BUTTON_A:
		if (deviceFeatures.fillModeNonSolid)
		{
			wireframe = !wireframe;
			reBuildCommandBuffers();
		}
		break;
	}
}

void VulkanExample::getOverlayText(VulkanTextOverlay* textOverlay)
{
	if (deviceFeatures.fillModeNonSolid)
	{
		textOverlay->addText("Press \"w\" to toggle wireframe", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
	}
}
