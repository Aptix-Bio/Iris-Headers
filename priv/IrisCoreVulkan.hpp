//
//  IrisCoreVulkan.h
//  Iris
//
//  Created by Ryan Landvater on 9/25/23.
//

#ifndef IrisCoreVulkan_h
#define IrisCoreVulkan_h

// Vulkan uses radiance and [0,1] range
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__

#if defined _WIN32
	// #define VK_NO_PROTOTYPES if using indirect loading
	#define VK_USE_PLATFORM_WIN32_KHR
	#include <vulkan/vulkan.h>
#elif defined __APPLE__
	#include <vulkan/vulkan.h>
	#include <vulkan/vulkan_metal.h>
	#if TARGET_OS_OSX
		#include <vulkan/vulkan_macos.h>
	#elif TARGET_OS_IOS
		#include <vulkan/vulkan_ios.h>
	#else
	#endif
#endif

#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif /* IrisCoreVulkan_h */
