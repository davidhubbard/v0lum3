/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <unistd.h>
#include <functional>
#include <typeinfo>
#include <cxxabi.h>

#pragma once

#ifndef VK_NULL_HANDLE
#error VkPtr.h requires <vulkan/vulkan.h> to be included before it.
#else

// class VkPtr wraps the object returned from some create...() function and
// automatically calls a destroy...() function when VkPtr is destroyed.
//
// In other words, VkPtr is just a wrapper around the destroy_fn, calling it
// at the right time.
//
// For example:
//   VkPtr<VkInstance> instance{vkDestroyInstance}
//   auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
//
// VkPtr has a member 'allocator' which is always nullptr. A derived class may
// set allocator to a custom allocator.
template <typename T>
class VkPtr {
public:
	// Constructor that has no destroy_fn. This is useful when VkPtr is embedded in a vector, etc. and must be initialized later.
	explicit VkPtr() {
		object = VK_NULL_HANDLE;
		deleter = [](T obj) { (void) obj; };
		allocator = nullptr;
	}

	// Constructor that has a destroy_fn which takes two arguments: the obj and the allocator.
	explicit VkPtr(std::function<void(T, VkAllocationCallbacks*)> destroy_fn) {
		object = VK_NULL_HANDLE;
		deleter = [this, destroy_fn](T obj) { destroy_fn(obj, allocator); };
		allocator = nullptr;
	}

	// Constructor that has a destroy_fn which takes 3 arguments: a VkInstance, the obj, and the allocator.
	// Note that the VkInstance is wrapped in a VkPtr itself.
	explicit VkPtr(const VkInstance& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> destroy_fn) {
		object = VK_NULL_HANDLE;
		deleter = [this, destroy_fn, &instance](T obj) { destroy_fn(instance, obj, allocator); };
		allocator = nullptr;
	}

	// Constructor that has a destroy_fn which takes 3 arguments: a VkDevice, the obj, and the allocator.
	// Note that the VkDevice is wrapped in a VkPtr itself.
	explicit VkPtr(const VkDevice& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> destroy_fn) {
		object = VK_NULL_HANDLE;
		deleter = [this, destroy_fn, &device](T obj) { destroy_fn(device, obj, allocator); };
		allocator = nullptr;
	}

	virtual ~VkPtr() {
		reset();
	}

	void reset() {
		if (object != VK_NULL_HANDLE) {
			deleter(object);
		}
		object = VK_NULL_HANDLE;
	}

	// Allow const access to the object.
	operator T() const {
		return object;
	}

	// Restrict non-const access. The object must be VK_NULL_HANDLE before it can be written. Call reset() if necessary.
	T* operator &() {
		if (object != VK_NULL_HANDLE) {
			int status;
			fprintf(stderr, "FATAL: VkPtr<%s>::operator& before reset()\n",
			        abi::__cxa_demangle(typeid(T).name(), 0, 0, &status));
			exit(1);
		}
		return &object;
	}

private:
	VkAllocationCallbacks * allocator;
	std::function<void(T)> deleter;
	T object;
};

#endif // ifdef VK_NULL_HANDLE

