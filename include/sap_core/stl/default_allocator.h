#pragma once
#include <memory>
// Projects may opt in to a tagged allocator for STL containers. Unconfigured
// sap_core continues to use std::allocator and its existing constructors.
#ifdef SAP_CORE_ALLOCATOR_HEADER
#include SAP_CORE_ALLOCATOR_HEADER
#endif
#ifndef SAP_CORE_DEFAULT_ALLOCATOR
#define SAP_CORE_DEFAULT_ALLOCATOR std::allocator
#endif
