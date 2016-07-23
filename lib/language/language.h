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
 *   GLFWwindow * window = glfwCreateWindow(...);
 *   unsigned int glfwExtensionCount = 0;
 *   const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
 *   language::Instance inst;
 *   if (inst.ctorError(glfwExtensions, glfwExtensionCount)) return 1;
 *   VkPtr<VkSurfaceKHR> surface{inst.vk, vkDestroySurfaceKHR};
 *   if (glfwCreateWindowSurface(inst.vk, window, nullptr, &surface) != VK_SUCCESS) {
 *     cerr << "glfwCreateWindowSurface() failed" << endl;
 *     exit(1);
 *   }
 *   int r = inst.open(surface, [&](std::vector<language::QueueRequest>& request) -> int {
 *       for (size_t dev_i = 0; dev_i < inst.devs.size(); dev_i++) {
 *         const auto& dev = inst.devs.at(dev_i);
 *         for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
 *           const auto& fam = dev.qfams.at(q_i);
 *           if (fam.surfaceSupport != PRESENT) continue;
 *           if (!(fam.vk.queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
 *
 *           // For example, if only one queue is needed.
 *           language::QueueRequest qr(dev_i, q_i);
 *           request.push_back(qr);  // Could also be written emplace_back(dev_i, q_i);
 *           return 0;
 *         }
 *       }
 *       cerr << "Error: No device has a suitable GRAPHICS queue." << endl;
 *       return 1;
 *     });
 *   if (r != 0) exit(1);
 *   while(!glfwWindowShouldClose(window)) {
 *     glfwPollEvents();
 *   }
 *   glfwDestroyWindow(window);
 *   return r;
 * }
 */

#pragma once

#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

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

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
enum SurfaceSupport {
	UNDEFINED = 0,
	NONE = 1,
	PRESENT = 2,
};

// QueueRequest communicates the physical device and queue family (within the device)
// being requested by devQuery() (passed to open(), below). devQuery() should push
// one QueueRequest instance for each queue.
typedef struct QueueRequest {
	uint32_t dev_index;
	uint32_t dev_qfam_index;
	float priority;

	// The default priority is the highest possible (1.0). Modify the instance after
	// creating it to change the priority. Or extend the struct with a derived class.
	QueueRequest(uint32_t dev_i, uint32_t dev_qfam_i) {
		dev_index = dev_i;
		dev_qfam_index = dev_qfam_i;
		priority = 1.0;
	}
	virtual ~QueueRequest() {};
} QueueRequest;

// QueueFamily wraps VkQueueFamilyProperties. QueueFamily also gives whether the QueueFamily
// can be used to "present" on the app surface (i.e. render to the screen).
// If surfaceSupport == NONE, only offscreen rendering is supported.
typedef struct QueueFamily {
	VkQueueFamilyProperties vk;
	SurfaceSupport surfaceSupport;  // Result of vkGetPhysicalDeviceSurfaceSupportKHR().
	std::vector<float> prios;  // populated only for mainloop.
	std::vector<VkQueue> queues;  // populated only for mainloop.
} QueueFamily;

// Device wraps the Vulkan logical and physical devices and a list of QueueFamily
// supported by the physical device. When devQuery() is called, Instance::devs are
// populated with phys and qfams, but dev (the logical device) and qfams.queues
// are not populated in devQuery().
//
// devQuery() should push the QueueRequest instances suitable for your app.
//
// devQuery() should also get available device extensions and call
// extensionRequests.push_back() to enable any extensions for your app.
//
// Instance::open() uses devQuery() to set up each Device. When devQuery() returns,
// the QueueRequest vector is used to create Device::dev (the logical device).
// open() also populates Device::qfams.queues.
typedef struct Device {
	VkDevice dev;  // Logical Device. Unpopulated during devQuery().
	VkPhysicalDevice phys;  // Physical Device. Populated for devQuery().
	std::vector<QueueFamily> qfams;
	std::vector<const char *> extensionRequests;
} Device;

typedef std::function<int(std::vector<QueueRequest>& request)> devQueryFn;

// Instance holds the top level state of the Vulkan pipeline. Construct with
// ctorError() and abort if the return value is non-zero.
//
// The correct initialization procedure is:
// 1. Create an Instance object
// 2. Call ctorError() which loads requiredExtensions. Check the return value.
// 3. Construct the VkSurfaceKHR using whatever windowing library you choose.
// 4. Call Instance::open() to initialize devices and queue families.
// 5. Run the main loop.
//
// In many cases it only makes sense to use 1 CPU thread to submit to GPU queues
// even though the GPU consumes the commands out of the queue in parallel.
// Typically this happens if the GPU implements a single hardware port and the
// driver is forced to multiplex commands to that single port if the app uses
// multiple queues. In other words, the GPU is the bottleneck and is essentially
// single-threaded. Perhaps this could be viewed as a driver bug -- it should
// report a max of *1* queue only in this case.
//
// To be clear, queues are not about how the GPU consumes command buffers (the
// GPU consumes the commands in parallel). The CPU should assemble command buffers
// in parallel too. Queues *are* how Vulkan apps submit "draw calls," so it might
// make a difference e.g. in a multi-GPU system if a single queue is a bottleneck.
//
// Multi-GPU systems definitely allow multiple independent command queues, at least
// one queue per device.
typedef struct Instance {
	VkInstance vk;
	std::vector<Device> devs;

	// Dummy constructor.
	Instance() {};
	// This is the actual constructor (and can return an error if Vulkan indicates an error).
	WARN_UNUSED_RESULT int ctorError(const char ** requiredExtensions, size_t requiredExtensionCount);

	// open() enumerates devs and queue families and calls devQuery(). devQuery() should call
	// request.push_back() for at least one QueueRequest.
	WARN_UNUSED_RESULT int open(const VkSurfaceKHR& surface, devQueryFn devQuery);

	virtual ~Instance();
} Instance;

} // namespace language
