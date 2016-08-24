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
 *   int r = inst.open({WIDTH, HEIGHT});
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
// being requested by initQueues() (below). initQueues() should push one QueueRequest
// instance per queue you want created.
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
} QueueFamily;

// Forward declaration of Instance for Device.
class Instance;

// Device wraps the Vulkan logical and physical devices and a list of QueueFamily
// supported by the physical device. When initQueues() is called, Instance::devs
// are populated with phys and qfams, but Device::dev (the logical device) and
// Device::qfams.queues are not populated.
//
// initQueues() pushes QueueRequest instances for the queues your app needs.
//
// initQueues() should also get available device extensions and call
// extensionRequests.push_back() to enable any extensions for your app.
//
// initQueues() populates a QueueRequest vector used to create Device::dev (the
// logical device). open() then populates Device::qfams.queues.
typedef struct Device {
	Device() = default;
	Device(Device&&) = default;
	Device(const Device&) = delete;

	// Logical Device. Unpopulated until after open().
	VkPtr<VkDevice> dev{vkDestroyDevice};

	// Physical Device. Populated after ctorError().
	VkPhysicalDevice phys = VK_NULL_HANDLE;

	// Device extensions to choose from. Populated after ctorError().
	std::vector<VkExtensionProperties> availableExtensions;

	// qfams is populated after ctorError() but qfams.queue is unpopulated.
	std::vector<QueueFamily> qfams;
	// getQfamI() is a convenience method to get the queue family index that
	// supports the given SurfaceSupport. Returns (size_t) -1 on error.
	size_t getQfamI(SurfaceSupport support) const;

	// Request device extensions by adding to extensionRequests before open().
	std::vector<const char *> extensionRequests;

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;
	VkSurfaceFormatKHR format = { (VkFormat) 0, (VkColorSpaceKHR) 0 };
	VkPresentModeKHR mode = (VkPresentModeKHR) 0;
	VkExtent2D swapchainExtent;

	// Populate after open().
	VkPtr<VkSwapchainKHR> swapchain{dev, vkDestroySwapchainKHR};
	std::vector<VkImage> images;
	std::vector<Framebuf> framebufs;
} Device;

// Instance holds the root of the Vulkan pipeline. Constructor (ctor) is a
// 3-phase process:
// 1. Create an Instance object
// 2. Call ctorError() -- phase 2 of the ctor -- always check the error return.
// 3. Construct a VkSurfaceKHR using your choice of windowing library.
// 4. Call open() -- phase 3 of the ctor -- to init devices and queues.
// 5. Instance is now fully constructed.
//
// Some discussion about setting up queues:
//
// In many cases it only makes sense to use 1 CPU thread to submit to GPU queues
// even though the GPU can execute the commands in parallel. What happens is the
// GPU only has a single hardware port and Vulkan is forced to multiplex commands
// to that single port when the app starts using multiple queues. In other words,
// the GPU hardware port may be "single-threaded."
//
// Web resources: https://lunarg.com/faqs/command-multi-thread-vulkan/
//                https://forums.khronos.org/showthread.php/13172
//
// lib/language does not enforce a limit of 1 GRAPHICS queue because Vulkan
// has no limit either. To try out multiple queues, override Instance::initQueues()
// and add multiple instances of QueueRequest to the request vector.
class Instance {
public:
	Instance() = default;
	Instance(Instance&&) = delete;
	Instance(const Instance&) = delete;

	VkPtr<VkInstance> vk{vkDestroyInstance};
	VkPtr<VkSurfaceKHR> surface{vk, vkDestroySurfaceKHR};

	// This is also the constructor: vulkan errors are returned from ctorError().
	// requiredExtentions is an array of strings naming any required extensions
	// (e.g. glfwGetRequiredInstanceExtensions, the SDL story is not as well
	// defined yet but might be SDL_GetVulkanInstanceExtensions)
	WARN_UNUSED_RESULT int ctorError(const char ** requiredExtensions, size_t requiredExtensionCount);

	// open() is the third phase of the ctor. Call open() after initializing
	// 'surface' in your app (e.g. glfwCreateWindowSurface, SDL_CreateVulkanSurface)
	// surfaceSizeRequest is the initial size of the window.
	WARN_UNUSED_RESULT int open(VkExtent2D surfaceSizeRequest);

	virtual ~Instance();

	size_t devs_size() const { return devs.size(); };
	Device& at(size_t i) { return devs.at(i); };

	// requestQfams() is a convenience function. It selects the minimal list of
	// QueueFamily instances from Device dev_i and returns a vector of
	// QueueRequest that cover the requested support.
	//
	// For example:
	// auto r = dev.requestQfams(dev_i, {language::PRESENT, language::GRAPHICS});
	//
	// After requestQfams() returns, multiple queues can be obtained by adding
	// the QueueRequest multiple times in devQuery().
	std::vector<QueueRequest> requestQfams(
		size_t dev_i, std::set<SurfaceSupport> support);

	virtual int initDebug();
	// pDestroyDebugReportCallbackEXT is located in the vulkan library at startup.
	PFN_vkDestroyDebugReportCallbackEXT pDestroyDebugReportCallbackEXT = nullptr;
	VkDebugReportCallbackEXT debugReport = VK_NULL_HANDLE;

protected:
	// After the devs vector is created in open(), it must not be resized. Any
	// operation on the vector that causes it to reallocate its storage will
	// invalidate references held to the individual Device instances.
	std::vector<Device> devs;

	// Override initQueues() if you app needs more than one queue.
	// initQueues() should call request.push_back() for at least one QueueRequest.
	virtual int initQueues(std::vector<QueueRequest>& request);

	// Override initSurfaceFormat() if your app needs a different default.
	virtual int initSurfaceFormat(Device& dev);

	// Override initPresentMode() if your app needs a different default.
	virtual int initPresentMode(Device& dev);

	// Override createSwapchain() if your app needs a different swapchain.
	virtual int createSwapchain(size_t dev_i, VkExtent2D surfaceSizeRequest);
};

} // namespace language
