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
 *       bool foundDev = false;
 *       for (size_t dev_i = 0; dev_i < inst.devs.size(); dev_i++) {
 *         auto selected = inst.requestQfams(dev_i,
 *             {language::PRESENT, language::GRAPHICS});
 *         foundDev |= selected.size();
 *         request.insert(request.end(), selected.begin(), selected.end());
 *       }
 *       if (!foundDev) {
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
#include <set>
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

// Forward declaration of Device for Framebuffer.
struct Device;

// Framebuffer references the presented pixels and manages the memory behind them.
typedef struct Framebuf {
	Framebuf(Device& dev);
	Framebuf(Framebuf&&) = default;
	Framebuf(const Framebuf&) = delete;

	VkPtr<VkImageView> imageView;
	VkPtr<VkFramebuffer> vk;
} Framebuf;

// SurfaceSupport encodes the result of vkGetPhysicalDeviceSurfaceSupportKHR().
// As an exception, the GRAPHICS value is used to request a QueueFamily
// with vk.queueFlags & VK_QUEUE_GRAPHICS_BIT in Instance::requestQfams() and
// Device::getQfamI().
enum SurfaceSupport {
	UNDEFINED = 0,
	NONE = 1,
	PRESENT = 2,

	GRAPHICS = 0x1000,  // Not used in struct QueueFamily.
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
// SurfaceSupport is the result of vkGetPhysicalDeviceSurfaceSupportKHR().
typedef struct QueueFamily {
	QueueFamily(const VkQueueFamilyProperties& vk_, SurfaceSupport surfaceSupport_)
		: vk(vk_), surfaceSupport(surfaceSupport_) {};
	QueueFamily(QueueFamily&&) = default;
	QueueFamily(const QueueFamily&) = delete;

	VkQueueFamilyProperties vk;
	const SurfaceSupport surfaceSupport;
	inline bool isGraphics() const {
		return vk.queueFlags & VK_QUEUE_GRAPHICS_BIT;
	};

	// populated only for mainloop.
	std::vector<float> prios;
	std::vector<VkQueue> queues;
	std::vector<VkPtr<VkCommandPool>> pools;
} QueueFamily;

// Forward declaration of Instance for Device.
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
	// getQfamI() is a convenience method to get the queue family index that
	// supports the given SurfaceSupport. Returns (size_t) -1 on error.
	size_t getQfamI(SurfaceSupport support) const;

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
	std::vector<Framebuf> framebufs;
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
	Instance() = default;
	Instance(Instance&&) = delete;
	Instance(const Instance&) = delete;

	VkPtr<VkInstance> vk{vkDestroyInstance};
	VkPtr<VkSurfaceKHR> surface{vk, vkDestroySurfaceKHR};

	// This is the actual constructor (and can return an error if Vulkan indicates an error).
	WARN_UNUSED_RESULT int ctorError(const char ** requiredExtensions, size_t requiredExtensionCount);

	// open() enumerates devs and queue families and calls devQuery(). devQuery() should call
	// request.push_back() for at least one QueueRequest.
	// surfaceSizeRequest should be the size of the window.
	WARN_UNUSED_RESULT int open(VkExtent2D surfaceSizeRequest,
		devQueryFn devQuery);

	virtual ~Instance();

	// requestQfams() is a convenience function that selects the minimal list of
	// QueueFamily instances from Device dev_i and returns a vector of
	// QueueRequest that cover the requested support.
	//
	// For example:
	// auto r = dev.requestQfams({language::PRESENT, language::GRAPHICS});
	std::vector<QueueRequest> requestQfams(
		size_t dev_i, std::set<SurfaceSupport> support);

	// INTERNAL: Avoid calling initSurfaceFormat() directly.
	// Override initSurfaceFormat() if your app needs a different default.
	// Note: devQueryFn() can also change the format.
	virtual int initSurfaceFormat(Device& dev);

	// INTERNAL: Avoid calling initPresentMode() directly.
	// Override initPresentMode() if your app needs a different default.
	// Note: devQueryFn() can also change the mode.
	virtual int initPresentMode(Device& dev);

	PFN_vkDestroyDebugReportCallbackEXT pDestroyDebugReportCallbackEXT = nullptr;
	VkDebugReportCallbackEXT debugReport = nullptr;

	size_t devs_size() const { return devs.size(); };
	Device& at(size_t i) { return devs.at(i); };
protected:
	// After the devs vector is created, it must not be resized because any
	// operation on the vector that causes it to reallocate its storage
	// will invalidate references held to the individual Device instances.
	std::vector<Device> devs;

	// Override createSwapchain() if your app needs a different swapchain.
	virtual int createSwapchain(size_t dev_i, VkExtent2D surfaceSizeRequest);
} Instance;

} // namespace language
