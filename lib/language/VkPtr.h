/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <stdlib.h>
#include <typeinfo>
#include <cxxabi.h>

#pragma once

#ifndef VK_NULL_HANDLE
#error VkPtr.h requires <vulkan/vulkan.h> to be included before it.
#else

#define VKDEBUG(x...)
#ifndef VKDEBUG
#define VKDEBUG(x...) { auto typeID = getTypeID_you_must_free_the_return_value(); fprintf(stderr, x); free(typeID); }
#endif

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
	VkPtr() = delete;
	VkPtr(VkPtr&&) = default;

	explicit VkPtr(const VkPtr& other) {
		VKDEBUG("this=%p VkPtr<%s> copy from %p dD=%p\n", this, typeID, &other, other.deleterDev);
		memmove(this, &other, sizeof(*this));
		VKDEBUG("this=%p object=%p dT=%p dI=%p dD=%p  pI=%p pD=%p\n",
			this, *((void **) &this->object), deleterT, deleterInst, deleterDev, pInst, pDev);
	}

	// Constructor that has a destroy_fn which takes two arguments: the obj and the allocator.
	explicit VkPtr(void (* destroy_fn)(T, const VkAllocationCallbacks *)) {
		object = VK_NULL_HANDLE;
		if (!destroy_fn) {
			auto typeID = getTypeID_you_must_free_the_return_value();
			fprintf(stderr, "VkPtr<%s>::VkPtr(T,allocator) with destroy_fn=%p\n", typeID, destroy_fn);
			free(typeID);
			exit(1);
		}
		deleterT = destroy_fn;
		deleterInst = nullptr;
		deleterDev = nullptr;
		allocator = nullptr;
		pInst = nullptr;
		pDev = nullptr;
	}

	// Constructor that has a destroy_fn which takes 3 arguments: a VkInstance, the obj, and the allocator.
	// Note that the VkInstance is wrapped in a VkPtr itself.
	explicit VkPtr(VkPtr<VkInstance>& instance, void (* destroy_fn)(VkInstance, T, const VkAllocationCallbacks *)) {
		object = VK_NULL_HANDLE;
		if (!destroy_fn) {
			auto typeID = getTypeID_you_must_free_the_return_value();
			fprintf(stderr, "VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n", typeID, destroy_fn);
			free(typeID);
			exit(1);
		}
		VKDEBUG("this=%p VkPtr<%s>::VkPtr(inst,T,allocator) with destroy_fn=%p\n", this, typeID, destroy_fn);
		deleterT = nullptr;
		deleterInst = destroy_fn;
		deleterDev = nullptr;
		allocator = nullptr;
		pInst = &instance.object;
		pDev = nullptr;
	}

	// Constructor that has a destroy_fn which takes 3 arguments: a VkDevice, the obj, and the allocator.
	// Note that the VkDevice is wrapped in a VkPtr itself.
	explicit VkPtr(VkPtr<VkDevice>& device, void (* destroy_fn)(VkDevice, T, const VkAllocationCallbacks *)) {
		object = VK_NULL_HANDLE;
		if (!destroy_fn) {
			auto typeID = getTypeID_you_must_free_the_return_value();
			fprintf(stderr, "VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p\n", typeID, destroy_fn);
			free(typeID);
			exit(1);
		}
		VKDEBUG("this=%p VkPtr<%s>::VkPtr(dev,T,allocator) with destroy_fn=%p object=%p\n", this, typeID, destroy_fn, *((void **) &object));
		deleterT = nullptr;
		deleterInst = nullptr;
		deleterDev = destroy_fn;
		allocator = nullptr;
		pInst = nullptr;
		pDev = &device.object;
	}

	virtual ~VkPtr() {
		reset();
	}

	void reset() {
		if (object == VK_NULL_HANDLE) {
			return;
		}
		if (deleterT) {
			VKDEBUG("VkPtr<%s>::reset() calling deleterT(%p, allocator)\n", typeID, *((void **) &object));
			deleterT(object, allocator);
		} else if (deleterInst) {
			VKDEBUG("VkPtr<%s>::reset() calling deleterInst(inst=%p, %p, allocator)\n", typeID, pInst, *((void **) &object));
			deleterInst(*pInst, object, allocator);
		} else if (deleterDev) {
			VKDEBUG("VkPtr<%s>::reset() calling deleterDev(dev=%p, %p, allocator)\n", typeID, pDev, *((void **) &object));
			deleterDev(*pDev, object, allocator);
		} else {
			VKDEBUG("this=%p VkPtr<%s> deleter = null object = %p!\n", this, typeID, *((void **) &object));
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
			auto typeID = getTypeID_you_must_free_the_return_value();
			fprintf(stderr, "FATAL: VkPtr<%s>::operator& before reset()\n", typeID);
			free(typeID);
			exit(1);
		}
		return &object;
	}

	T object;

private:
	void (* deleterT)(T, const VkAllocationCallbacks *);
	void (* deleterInst)(VkInstance, T, const VkAllocationCallbacks *);
	void (* deleterDev)(VkDevice, T, const VkAllocationCallbacks *);
	const VkAllocationCallbacks * allocator;
	VkInstance * pInst;
	VkDevice * pDev;

	char * getTypeID_you_must_free_the_return_value() {
		int status;
		return abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
	}
};

#endif // ifdef VK_NULL_HANDLE
