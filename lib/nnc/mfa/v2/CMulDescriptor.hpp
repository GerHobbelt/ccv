#ifndef MFA_CMULDESCRIPTOR_HPP_
#define MFA_CMULDESCRIPTOR_HPP_

#include <simd/simd.h>
#include <utility>
#include "PipelineValue.hpp"
#include "DeviceProperties.hpp"
#include "GEMMOperandPrecision.hpp"

struct CMulKernelDescriptor {
  GEMMOperandPrecision memoryPrecision;
  unsigned int value;
  constexpr bool operator==(const CMulKernelDescriptor &rhs) const { return value == rhs.value && memoryPrecision == rhs.memoryPrecision; }
};

template<>
struct std::hash<CMulKernelDescriptor>
{
  std::size_t operator()(const CMulKernelDescriptor& hash) const noexcept { return (size_t)hash.value; }
};

struct CMulKernel;

struct CMulDescriptor {
  unsigned int value;

  GEMMOperandPrecision memoryPrecision;

  simd::uint3 stridesA;

  simd::uint3 stridesB;

  simd::uint3 stridesC;

  simd::uint3 dimensions;

  bool operator==(const CMulDescriptor& rhs) const;

  std::pair<CMulKernelDescriptor, PipelineValue<CMulKernel> *> findKernel(MTL::Device* const device, const DeviceProperties &dprops, std::unordered_map<CMulKernelDescriptor, std::unique_ptr<CMulKernel>> *const libraryCache) const noexcept;
};

template<>
struct std::hash<CMulDescriptor>
{
  std::size_t operator()(const CMulDescriptor& hash) const noexcept;
};

#endif

