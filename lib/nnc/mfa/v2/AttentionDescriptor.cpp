#include "AttentionDescriptor.hpp"
#include "AttentionKernelDescriptor.hpp"
#include "AttentionKernel.hpp"
#include "../ccv_nnc_mfa_hash.hpp"
#include "../ccv_nnc_mfa_error.hpp"

bool AttentionDescriptor::operator==(const AttentionDescriptor& rhs) const {
  return
  (lowPrecisionInputs == rhs.lowPrecisionInputs) &&
  (lowPrecisionIntermediates == rhs.lowPrecisionIntermediates) &&
  simd_all(matrixDimensions == rhs.matrixDimensions) &&
  simd_all(transposeState == rhs.transposeState);
}

std::size_t std::hash<AttentionDescriptor>::operator()(const AttentionDescriptor& hash) const noexcept {
  std::size_t seed = 0;
  using namespace ccv::nnc::mfa::hash;
  combine_32(seed, hash.matrixDimensions[0]);
  combine_32(seed, hash.matrixDimensions[1]);
  combine_32(seed, hash.matrixDimensions[2]);
  combine_32(seed, pack_32(simd::uchar4 { hash.transposeState[0], hash.transposeState[1], hash.transposeState[2], hash.transposeState[3] }));
  combine_32(seed, pack_32(simd::uchar4 { hash.lowPrecisionInputs, hash.lowPrecisionIntermediates, 0, 0 }));
  combine_32(seed, pack_32(simd::ushort2 { hash.type.value, 0 } ));
  return seed;
}

AttentionKernelDescriptor AttentionDescriptor::kernelDescriptor(MTL::Device *const device, const DeviceProperties &dprops) const noexcept {
  auto createHeadDimension = 
  [=]() -> unsigned short {
    return matrixDimensions[2];
  };
  auto createBlockDimensions =
  [=]() -> simd::ushort3 {
    unsigned short parallelization = 16;
    unsigned short traversal = 64; // 128;
    unsigned short originalHead = 16;
    // Enforce the rule that head block dimension <= head dimension.
    unsigned short headDimension = createHeadDimension();
    unsigned short paddedHeadDimension = (headDimension + 7) / 8 * 8;
    unsigned short revisedHead = std::min(originalHead, paddedHeadDimension);
 
    return simd::ushort3 { parallelization, traversal, revisedHead };
  };
  
  auto createCacheState =
  [=]() -> AttentionOperands<bool> {
    AttentionOperands<bool> output;
    switch (type.value) {
    case AttentionKernelType::forward:
      output[AttentionOperand::Q] = false;
      output[AttentionOperand::O] = true; // false;
      break;
    case AttentionKernelType::backwardQuery:
      output[AttentionOperand::Q] = false;
      output[AttentionOperand::dO] = false;
      output[AttentionOperand::dQ] = false;
      break;
    case AttentionKernelType::backwardKeyValue:
      output[AttentionOperand::K] = false;
      output[AttentionOperand::V] = false;
      output[AttentionOperand::dV] = false;
      output[AttentionOperand::dK] = false;
      break;
    }
    return output;
  };
  
  auto createTransposeState =
  [=]() -> AttentionOperands<bool> {
    AttentionOperands<bool> output;
    output[AttentionOperand::Q] = transposeState[0];
    output[AttentionOperand::K] = transposeState[1];
    output[AttentionOperand::V] = transposeState[2];
    output[AttentionOperand::O] = transposeState[3];
 
    output[AttentionOperand::dO] = transposeState[3];
    output[AttentionOperand::dV] = transposeState[2];
    output[AttentionOperand::dK] = transposeState[1];
    output[AttentionOperand::dQ] = transposeState[0];
    return output;
  };

  if (device->supportsFamily(MTL::GPUFamily(1009))) {
    return AttentionKernelDescriptor(createBlockDimensions(), createCacheState(), createHeadDimension(), createMemoryPrecisions(), true, false, createRegisterPrecisions(device), createTransposeState(), type);
  } else {
    return AttentionKernelDescriptor(createBlockDimensions(), createCacheState(), createHeadDimension(), createMemoryPrecisions(), false, true, createRegisterPrecisions(device), createTransposeState(), type);
  }
}

std::pair<AttentionKernelDescriptor, PipelineValue<AttentionKernel> *> AttentionDescriptor::findKernel(MTL::Device *const device, const DeviceProperties &dprops, std::unordered_map<AttentionKernelDescriptor, std::unique_ptr<AttentionKernel>> *const libraryCache) const noexcept {
  auto createPipeline =
  [=](MTL::Library* library) -> MTL::ComputePipelineState* {
    // Set the function constants.
    auto constants = NS::TransferPtr
    (MTL::FunctionConstantValues::alloc()->init());
    uint32_t rowDimension = matrixDimensions[0];
    uint32_t columnDimension = matrixDimensions[1];
    constants->setConstantValue(&rowDimension, MTL::DataTypeUInt, NS::Integer(0));
    constants->setConstantValue(&columnDimension, MTL::DataTypeUInt, 1);

    NS::String* swiftName = NS::String::string("attention", NS::UTF8StringEncoding);
    NS::Error* error = nil;
    
    auto function = NS::TransferPtr
    (library->newFunction(swiftName, constants.get(), &error));
    CCV_NNC_MFA_CHECK_ERROR(error);
    
    auto pipeline = device->newComputePipelineState(function.get(), &error);
    CCV_NNC_MFA_CHECK_ERROR(error);
    return pipeline;
  };

  auto createKernel =
  [=](AttentionKernelDescriptor descriptor) -> AttentionKernel* {
    auto iterator = libraryCache->find(descriptor);
    if (iterator != libraryCache->end()) {
      return iterator->second.get();
    } else {
      AttentionKernel* kernel = new AttentionKernel(descriptor, device);
      (*libraryCache)[descriptor] = std::unique_ptr<AttentionKernel>(kernel);
      return kernel;
    }
  };

  auto kernelDesc = kernelDescriptor(device, dprops);
  AttentionKernel* kernel = createKernel(kernelDesc);
  auto pipeline = NS::TransferPtr(createPipeline(kernel->library.get()));
    
  // Force the user to retrieve the return value from the cache. We ensure
  // the cache takes ownership, and the pointer doesn't become a zombie
  // object.
  PipelineValue<AttentionKernel>* output = new PipelineValue<AttentionKernel> { kernel, pipeline };
  return std::make_pair(kernelDesc, output);
}

AttentionOperands<GEMMOperandPrecision> AttentionDescriptor::createMemoryPrecisions() const noexcept {
  AttentionOperands<GEMMOperandPrecision> memoryPrecisions;
  
  if (lowPrecisionInputs) {
    memoryPrecisions[AttentionOperand::Q] = GEMMOperandPrecision::FP16;
    memoryPrecisions[AttentionOperand::K] = GEMMOperandPrecision::FP16;
    memoryPrecisions[AttentionOperand::V] = GEMMOperandPrecision::FP16;
    memoryPrecisions[AttentionOperand::dO] = GEMMOperandPrecision::BF16;
  } else {
    memoryPrecisions[AttentionOperand::Q] = GEMMOperandPrecision::FP32;
    memoryPrecisions[AttentionOperand::K] = GEMMOperandPrecision::FP32;
    memoryPrecisions[AttentionOperand::V] = GEMMOperandPrecision::FP32;
    memoryPrecisions[AttentionOperand::dO] = GEMMOperandPrecision::FP32;
  }
  
  // Rounding error. In the test that reported these errors, the average
  // magnitude of any scalar was typically 1.0 to 10.0.
  //
  //   | FP32 | FP16/BF16   |
  // - | ---- | ----------- |
  // L | 2e-5 | 7e-3 (FP16) |
  // D | 2e-5 | 1e-1 (BF16) |
  //
  // Although the error in D is relatively large (1e-1), it does not impact
  // the error of the final outputs (O/dV/dK/dQ). For example, the error of
  // O/dV/dK/dQ is always 5e-2 in typical mixed precision workflows.
  // When D is demoted to BF16, the error of O/dV/dK/dQ is still 5e-2.
  //
  // Benchmarks suggest that keeping D in BF16, measurably improves ALU
  // utilization in the backward dK/dV pass. Samples were taken at every
  // whole number head dimension from 32 to 96 (e.g. 32, 33, 34, ...) and a
  // constant sequence length. The improvement was ~1% on both architectures.
  //
  // M1 Max, Sequence Dimension = 8192
  //
  // |         | BWD    | dQ     | dK/dV  |
  // | ------- | ------ | ------ | ------ |
  // | Average |  0.0%  | +0.1%  | +1.1%  |
  // | Minimum | -0.2%  | -1.2%  | -1.9%  |
  // | Median  |  0.0%  |  0.0%  | +1.4%  |
  // | Maximum | +0.2%  | +4.4%  | +5.6%  |
  //
  // M4, Sequence Dimension = 4096
  //
  // |         | BWD    | dQ     | dK/dV  |
  // | ------- | ------ | ------ | ------ |
  // | Average |  0.0%  |  0.0%  | +0.8%  |
  // | Minimum | -0.4%  | -0.2%  | -0.1%  |
  // | Median  |  0.0%  |  0.0%  | +0.8%  |
  // | Maximum |  0.3%  | +0.2%  | +3.0%  |
  //
  // To confirm this conclusion, a second study was performed on M1 Max at
  // large head dimensions (95 to 160). In addition, examining only the
  // subset of head dimensions that divide evenly by 8.
  //
  // M1 Max, dK/dV
  //
  // |         | 32 to 96 | 96 to 160 | 32 to 160 (div. 8) |
  // | ------- | -------- | --------- | ------------------ |
  // | Average | +1.1%    | +0.3%     | +0.6%              |
  // | Minimum | -1.9%    | -1.5%     | -1.5%              |
  // | Median  | +1.4%    | +0.2%     | +0.0%              |
  // | Maximum | +5.6%    | +2.5%     | +5.6%              |
  //
  // The improvement diminishes to ~0.3% at larger head dimensions. This
  // makes sense, as the overhead of one elementwise operation is amortized
  // over a larger dot product. The head dimension increased 2x and the
  // improvement shrunk 2-3x. For heads divisible by 8 (the target use case),
  // the improvement shrunk from major at small heads, to zero at large
  // ones. The cutoff aligns with the point where the GEMM loops cannot be
  // unrolled (head dimension vastly exceeds head block dimension).
  if (lowPrecisionIntermediates) {
    memoryPrecisions[AttentionOperand::L] = GEMMOperandPrecision::FP16;
    memoryPrecisions[AttentionOperand::D] = GEMMOperandPrecision::BF16;
  } else {
    memoryPrecisions[AttentionOperand::L] = GEMMOperandPrecision::FP32;
    memoryPrecisions[AttentionOperand::D] = GEMMOperandPrecision::FP32;
  }
  
  // Data for low precision outputs.
  //
  // Traversal block = 64, sequence length = 256, head size = 32
  // FP16 (O)          | cached: 3e-4 | paged: 5e-4   | 2x
  // BF16 (dV, dK, dQ) | cached: 4e-3 | paged: 1.3e-2 | 3x
  //
  // Traversal block = 64, sequence length = 1024, head size = 32
  // FP16 (O)          | cached: 2e-4 | paged: 5e-4   | 3x
  // BF16 (dV, dK, dQ) | cached: 4e-3 | paged: 1.5e-2 | 4x
  //
  // Traversal block = 64, sequence length = 4096, head size = 32
  // FP16 (O)          | cached: 1e-4 | paged: 5e-4   | 5x
  // BF16 (dV, dK, dQ) | cached: 1e-3 | paged: 4e-2   | 40x
  //
  // Traversal block = 64, sequence length = 8192, head size = 32
  // FP16 (O)          | cached: 4e-5 | paged: 5e-4   | 13x
  // BF16 (dV, dK, dQ) | cached: 1e-3 | paged: 4e-2   | 40x
  //
  // The benchmarks were taken in the case where O/dV/dK/dQ are spilled to
  // memory. Hence, the impact of writing them to memory scales with N^2.
  // M1 was slower when packing/unpacking BF16, while M4 was faster. This
  // was without utilizing the native hardware instructions for BF16 to
  // FP32 conversion on M4.
  //
  // M4 is faster when the accumulators are stored in registers, up to at
  // least head dimension 256. The cost of storing scales with N on that
  // architecture. BF16 would only bring harm on M1 and no change on M3 with
  // proper heuristics. I am forcing dV/dK/dQ to be stored in RAM as FP32,
  // based on performance alone (although it does help the rounding error).
  //
  // Clients can issue a subsequent kernel that casts the FP32 scalars to
  // BF16, within a smaller memory allocation. Then, deallocate the FP32
  // allocation. The overall training process will not be any slower than
  // if MFA outputted BF16 into the final buffer.
  //
  // ## Update
  //
  // Paging O as FP16 was found to be slower on M1. Like with BF16, the M3
  // generation was faster. Writing O directly to FP16 is a very
  // important use case: attention inference. Small head dimensions fit
  // inside the registers and don't convert FP16 -> FP32 -> FP16 every loop
  // iteration. They only convert once at the end of the kernel. It is the
  // supermassive head dimensions that require register spilling, and
  // therefore an FP32 memory allocation for O.
  //
  // I don't know the best way to resolve this. It seems like something the
  // client should deal with. Therefore, the MFA reference implementation
  // will always write O as FP32 in memory. This choice simplifies
  // everything, just like the choice to always store log-sum-exp during the
  // forward pass. It also removes the concern of rounding error from
  // frequently truncating the FP32 numbers to FP16.
  memoryPrecisions[AttentionOperand::O] = GEMMOperandPrecision::FP32;
  memoryPrecisions[AttentionOperand::dV] = GEMMOperandPrecision::FP32;
  memoryPrecisions[AttentionOperand::dK] = GEMMOperandPrecision::FP32;
  memoryPrecisions[AttentionOperand::dQ] = GEMMOperandPrecision::FP32;
  
  return memoryPrecisions;
}

AttentionOperands<GEMMOperandPrecision> AttentionDescriptor::createRegisterPrecisions(MTL::Device *const device) const noexcept {
  AttentionOperands<GEMMOperandPrecision> registerPrecisions;
  
  // Query whether the hardware fuses the promotion of BF16 to FP32 with
  // the FMA assembly instruction.
  const bool hasNativeBF16Casting = device->supportsFamily(MTL::GPUFamily(1009));
  
  // Inputs have the same register precision across kernels.
  if (lowPrecisionInputs) {
    registerPrecisions[AttentionOperand::Q] = GEMMOperandPrecision::FP16;
    registerPrecisions[AttentionOperand::K] = GEMMOperandPrecision::FP16;
    registerPrecisions[AttentionOperand::V] = GEMMOperandPrecision::FP16;
    registerPrecisions[AttentionOperand::dO] = hasNativeBF16Casting ? GEMMOperandPrecision::BF16 : GEMMOperandPrecision::FP32;
  } else {
    registerPrecisions[AttentionOperand::Q] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::K] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::V] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::dO] = GEMMOperandPrecision::FP32;
  }
  
  // The register precision of L/D only counts for backward key-value.
  if (lowPrecisionIntermediates) {
    registerPrecisions[AttentionOperand::L] = GEMMOperandPrecision::FP16;
    registerPrecisions[AttentionOperand::D] = hasNativeBF16Casting ? GEMMOperandPrecision::BF16 : GEMMOperandPrecision::FP32;
  } else {
    registerPrecisions[AttentionOperand::L] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::D] = GEMMOperandPrecision::FP32;
  }
  
  // The register precision for the attention matrix.
  if (lowPrecisionIntermediates) {
    // There is a special FP16xFP16->FP16 instruction that reaches peak ALU
    // throughput. S = Q * K is the only place where it can be employed
    // in attention kernels.
    //
    // S = Q * K is the most often recomputed intermediate (3 out of 9 GEMMs,
    // 2 out of 3 unnecessary GEMMs). If we optimize this, the impact on
    // performance will be greater than for any other multiplication.
    //
    // Accumulating S in FP16 increased the rounding error tenfold in one
    // experiment (5e-3 to 5e-2). For reference, the average magnitude of any
    // scalar was 1.0 to 10.0.
    //
    // FP16 (Q, K)    | 5e-3
    // FP16 (Q, K, S) | 5e-2
    // FP16 (P)       | 2.7e-3
    // BF16 (dS)      | 8e-3
    registerPrecisions[AttentionOperand::S] = lowPrecisionInputs ? GEMMOperandPrecision::FP16 : GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::P] = GEMMOperandPrecision::FP16;
    registerPrecisions[AttentionOperand::dP] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::dS] = hasNativeBF16Casting ? GEMMOperandPrecision::BF16 : GEMMOperandPrecision::FP32;
  } else {
    registerPrecisions[AttentionOperand::S] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::P] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::dP] = GEMMOperandPrecision::FP32;
    registerPrecisions[AttentionOperand::dS] = GEMMOperandPrecision::FP32;
  }
  
  // All of the outputs are accumulated in FP32.
  registerPrecisions[AttentionOperand::O] = GEMMOperandPrecision::FP32;
  registerPrecisions[AttentionOperand::dV] = GEMMOperandPrecision::FP32;
  registerPrecisions[AttentionOperand::dK] = GEMMOperandPrecision::FP32;
  registerPrecisions[AttentionOperand::dQ] = GEMMOperandPrecision::FP32;
  
  return registerPrecisions;

}