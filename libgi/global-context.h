#pragma once

#include "context.h"

// if we at some point put in T as well as derived classes we have to adapt this
template<typename T> class dynamic_variable {
	std::vector<T> stack;
public:
    template<typename... Args> dynamic_variable(Args &&...args) {
		stack.emplace_back(std::forward<Args>(args)...);
	}
	T& operator*() {
		return stack.back();
	}
	const T& operator*() const {
		return stack.back();
	}
	T* operator->() {
		return &stack.back();
	}
	const T* operator->() const {
		return &stack.back();
	}
};

extern dynamic_variable<render_context> rc;
