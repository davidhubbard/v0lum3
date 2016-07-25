/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/language is the first-level language bindings for Vulkan.
 * lib/language is part of the v0lum3 project.
 * This library is called "language" as a homage to Star Trek First Contact.
 * Hopefully this "language" is easier than Vulkan.
 *
 * Vulkan is verbose -- but that is a good thing. lib/language handles the
 * initialization of the VkInstance, devices, queue selection, and extension
 * selection.
 *
 * Note: lib/language is designed to be used in source form! The functions
 * are kept short and sweet, so there are very likely some things your app
 * needs that are not exposed by the lib/language API. Clone lib/language
 * to your own repo and make changes. For example:
 * 1. Change the app name reported to vulkan.
 * 2. Use your app's logging facility instead of fprintf(stderr).
 * 3. Enable instance-level extensions. Or blacklist extensions.
 * 4. Use a custom allocator.
 * etc.
 *
 * The v0lum3 project is happy to incorporate useful changes back into the
 * upstream code.
 *
 * Example usage:
 *
 * // yourprogram.cpp:
 * #include <lib/language/language.h>
 *
 * int main() {
 *   const int WIDTH = 800, HEIGHT = 600;
 *   GLFWwindow * window = glfwCreateWindow(WIDTH, HEIGHT...);
 *   unsigned int glfwExtCount = 0;
 *   const char ** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
 *   language::Instance inst;
 *   if (inst.ctorError(glfwExts, glfwExtCount)) return 1;
 *   if (glfwCreateWindowSurface(inst.vk, window, nullptr, &inst.surface)
 *       != VK_SUCCESS) {
 *     cerr << "glfwCreateWindowSurface() failed" << endl;
 *     exit(1);
 *   }
 *   int r = inst.open({WIDTH, HEIGHT},
 *     [&](std::vector<language::QueueRequest>& request) -> int {
 *       bool foundPRESENTdev = false;
 *       for (size_t dev_i = 0; dev_i < inst.devs.size(); dev_i++) {
 *         const auto& dev = inst.devs.at(dev_i);
 *         bool foundPRESENT = false, foundGRAPHICS = false;
 *         for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
 *           const auto& fam = dev.qfams.at(q_i);
 *           bool fPRESENT = fam.surfaceSupport == PRESENT;
 *           bool fGRAPHICS = (fam.vk.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
 *           // Only need one queue to PRESENT and one to GRAPHICS.
 *           if ((!fPRESENT || foundPRESENT) && (!fGRAPHICS || foundGRAPHICS)) {
 *             continue;
 *           }
 *           foundPRESENT |= fPRESENT;
 *           foundGRAPHICS |= fGRAPHICS;
 *
 *           language::QueueRequest qr(dev_i, q_i);
 *           request.push_back(qr);  // Or emplace_back(dev_i, q_i);
 *         }
 *         if (foundPRESENT && foundGRAPHICS) {
 *           foundPRESENTdev = true;  // A single physical device must have both.
 *         }
 *       }
 *       if (!foundPRESENTdev) {
 *         cerr << "Error: no device has a suitable PRESENT queue." << endl;
 *         return 1;
 *       }
 *       return 0;
 *     });
 *   if (r != 0) exit(1);
 *   while(!glfwWindowShouldClose(window)) {
 *     glfwPollEvents();
 *   }
 *   glfwDestroyWindow(window);
 *   return r;
 * }
 */

#include <vector>
#include <functional>
#include <vulkan/vulkan.h>
#include "VkPtr.h"

#pragma once

namespace language {

#if defined(COMPILER_GCC)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

// For debugging lib/language: set language::dbg_lvl to higher values to log more debug info.
extern int dbg_lvl;
extern const char VK_LAYER_LUNARG_standard_validation[];

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
enum SurfaceSupport {
	UNDEFINED = 0,
	NONE = 1,
	PRESENT = 2,
};

// QueueRequest communicates the physical device and queue family (within the device)
// being requested by devQuery() (passed to open(), below). devQuery() should push
// one QueueRequest instance per queue you want created.
typedef struct QueueRequest {
	uint32_t dev_index;
	uint32_t dev_qfam_index;
	float priority;

	// The default priority is the highest possible (1.0), but can be changed.
	QueueRequest(uint32_t dev_i, uint32_t dev_qfam_i) {
		dev_index = dev_i;
		dev_qfam_index = dev_qfam_i;
		priority = 1.0;
	}
	virtual ~QueueRequest() {};
} QueueRequest;

// QueueFamily wraps VkQueueFamilyProperties. QueueFamily also gives whether the
// QueueFamily can be used to "present" on the app surface (i.e. swap a surface to
// the screen if surfaceSupport == PRESENT).
typedef struct QueueFamily {
	VkQueueFamilyProperties vk;
	SurfaceSupport surfaceSupport;  // Result of vkGetPhysicalDeviceSurfaceSupportKHR().
	std::vector<float> prios;  // populated only for mainloop.
	std::vector<VkQueue> queues;  // populated only for mainloop.
} QueueFamily;

struct Instance;

// Device wraps the Vulkan logical and physical devices and a list of QueueFamily
// supported by the physical device. When devQuery() is called, Instance::devs are
// populated with phys and qfams, but dev (the logical device) and qfams.queues
// are not populated.
//
// devQuery() should push the QueueRequest instances suitable for your app.
//
// devQuery() should also get available device extensions and call
// extensionRequests.push_back() to enable any extensions for your app.
//
// Instance::open() uses devQuery() to set up each Device. When devQuery() returns,
// the QueueRequest vector is used to create Device::dev (the logical device).
// open() then populates Device::qfams.queues and returns.
//
// surfaceFormats and presentModes are populated before devQuery() if the device
// claims to have a queue that supports PRESENT (and has the right extensions to
// do a swapchain). devQuery() may choose a different format or mode. But
// format and mode should just work automatically...devQuery() does not have
// to handle that.
typedef struct Device {
	Device() = default;
	Device(Device&&) = default;
	Device(const Device&) = delete;

	// Logical Device. Unpopulated during devQuery().
	VkPtr<VkDevice> dev{vkDestroyDevice};

	// Physical Device. Populated for devQuery().
	VkPhysicalDevice phys = VK_NULL_HANDLE;

	// Extensions devQuery() can choose from. Populated for devQuery().
	std::vector<VkExtensionProperties> availableExtensions;

	// qfams is populated for devQuery() but qfams.queue is unpopulated.
	std::vector<QueueFamily> qfams;

	// devQuery() can request extension names by adding to extensionRequests.
	std::vector<const char *> extensionRequests;

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;
	VkSurfaceFormatKHR format = { (VkFormat) 0, (VkColorSpaceKHR) 0 };
	VkPresentModeKHR mode = (VkPresentModeKHR) 0;
	VkExtent2D swapchainExtent;

	// Unpopulated during devQuery().
	VkPtr<VkSwapchainKHR> swapchain{dev, vkDestroySwapchainKHR};
	std::vector<VkImage> images;
	std::vector<VkPtr<VkImageView>> imageViews;
} Device;

// Type signature for devQuery():
// devQuery() should call request.push_back() for at least one QueueRequest.
typedef std::function<int(std::vector<QueueRequest>& request)> devQueryFn;

// Instance holds the top level state of the Vulkan pipeline. Construct with
// ctorError() and abort if the return value is non-zero.
//
// The correct initialization procedure is:
// 1. Create an Instance object
// 2. Call ctorError() which loads requiredExtensions. Check the return value.
// 3. Construct a VkSurfaceKHR using your choice of windowing library.
// 4. Call Instance::open() to initialize devices and queue families.
//    open() calls your devQuery() function.
// 5. Run the main loop.
//
// In many cases it only makes sense to use 1 CPU thread to submit to GPU queues
// even though the GPU consumes the commands out of the queue in parallel.
// Typically this happens if the GPU implements a single hardware port and the
// driver is forced to multiplex commands to that single port if the app uses
// multiple queues. In other words, the GPU is the bottleneck and is essentially
// single-threaded. Perhaps this could be viewed as a driver bug -- it should
// report a max of *1* queue only in this case. But some drivers report more.
//
// To be clear, queues are not about how the GPU consumes command buffers (the
// GPU consumes the commands in parallel). The CPU should assemble command buffers
// in parallel too. Queues *are* how Vulkan apps submit "draw calls," so it might
// make a difference e.g. in a multi-GPU system if a single queue is a bottleneck.
//
// Multi-GPU systems definitely allow multiple independent command queues, at least
// one queue per device.
//
// Separate GRAPHICS and PRESENT queues don't reduce the GRAPHICS queue load.
typedef struct Instance {
	VkPtr<VkInstance> vk{vkDestroyInstance};
	VkPtr<VkSurfaceKHR> surface{vk, vkDestroySurfaceKHR};
	std::vector<Device> devs;

	// This is the actual constructor (and can return an error if Vulkan indicates an error).
	WARN_UNUSED_RESULT int ctorError(const char ** requiredExtensions, size_t requiredExtensionCount);

	// open() enumerates devs and queue families and calls devQuery(). devQuery() should call
	// request.push_back() for at least one QueueRequest.
	// surfaceSizeRequest should be the size of the window.
	WARN_UNUSED_RESULT int open(VkExtent2D surfaceSizeRequest,
		devQueryFn devQuery);

	virtual ~Instance();

	// INTERNAL: Avoid calling initSurfaceFormat() directly.
	// Override initSurfaceFormat() if your app needs a different default.
	// Note: devQueryFn() can also change the format.
	virtual int initSurfaceFormat(Device& dev);

	// INTERNAL: Avoid calling initPresentMode() directly.
	// Override initPresentMode() if your app needs a different default.
	// Note: devQueryFn() can also change the mode.
	virtual int initPresentMode(Device& dev);

protected:
	// Override createSwapchain() if your app needs a different swapchain.
	virtual int createSwapchain(size_t dev_i, VkExtent2D surfaceSizeRequest);
} Instance;

} // namespace language
