/**
* Copyright (c) 2016-present, Facebook, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
/* Modifications Copyright (c) Microsoft. */

#include "core/providers/cpu/nn/lrn.h"
#include "core/util/math.h"
#include "core/util/math_cpuonly.h"

namespace onnxruntime {

template <>
Status LRN<float>::Compute(OpKernelContext* context) const {
  const Tensor* X = context->Input<Tensor>(0);
  if (X == nullptr) return Status(common::ONNXRUNTIME, common::FAIL, "input count mismatch");

  Tensor* Y = context->Output(0, X->Shape());

  // Supports NCHW image format.
  ORT_ENFORCE(X->Shape().NumDimensions() == 4);
  const int N = gsl::narrow_cast<int>(X->Shape()[0]);
  const int C = gsl::narrow_cast<int>(X->Shape()[1]);
  const int H = gsl::narrow_cast<int>(X->Shape()[2]);
  const int W = gsl::narrow_cast<int>(X->Shape()[3]);
  const int image_size = C * H * W;
  const int pre_pad = (size_ - 1) / 2;

  const float* Xdata = X->template Data<float>();
  float* Ydata = Y->template MutableData<float>();

  AllocatorPtr alloc;
  ORT_RETURN_IF_ERROR(context->GetTempSpaceAllocator(&alloc));

  const int Xsize = gsl::narrow_cast<int>(X->Shape().Size());
  auto sdata = alloc->Alloc(sizeof(float) * Xsize);
  BufferUniquePtr scale_buffer(sdata, BufferDeleter(alloc));
  float* scale_data = static_cast<float*>(scale_buffer.get());
  math::Set<float, CPUMathUtil>(Xsize, bias_, scale_data, &CPUMathUtil::Instance());

  const size_t padded_square_size = (C + size_ - 1) * H * W;
  auto psdata = alloc->Alloc(sizeof(float) * padded_square_size);
  BufferUniquePtr padded_square_buffer(psdata, BufferDeleter(alloc));
  float* padded_square_data = static_cast<float*>(padded_square_buffer.get());
  math::Set<float, CPUMathUtil>(padded_square_size, 0.0f, padded_square_data, &CPUMathUtil::Instance());

  const float alpha_over_size = alpha_ / size_;
  // go through the images
  for (int n = 0; n < N; ++n) {
    // compute the padded square
    math::Sqr<float, CPUMathUtil>(image_size, Xdata + image_size * n,
                                  padded_square_data + pre_pad * H * W,
                                  &CPUMathUtil::Instance());
    // Create the first channel scale
    for (int c = 0; c < size_; ++c) {
      math::Axpy<float, CPUMathUtil>(
          H * W, alpha_over_size, padded_square_data + c * H * W,
          scale_data + image_size * n, &CPUMathUtil::Instance());
    }

    for (int c = 1; c < C; ++c) {
      float* this_scale_slice = scale_data + n * image_size + c * H * W;
      // copy previous scale
      memcpy(this_scale_slice, this_scale_slice - H * W, H * W * sizeof(float));
      // add head
      math::Axpy<float, CPUMathUtil>(
          H * W, alpha_over_size, padded_square_data + (c + size_ - 1) * H * W,
          this_scale_slice, &CPUMathUtil::Instance());
      // subtract tail
      math::Axpy<float, CPUMathUtil>(
          H * W, -alpha_over_size, padded_square_data + (c - 1) * H * W,
          this_scale_slice, &CPUMathUtil::Instance());
    }
  }

  math::Powx<float, CPUMathUtil>(Xsize, scale_data, -beta_, Ydata, &CPUMathUtil::Instance());
  math::Mul<float, CPUMathUtil>(Xsize, Ydata, Xdata, Ydata, &CPUMathUtil::Instance());

  return Status::OK();
}

ONNX_CPU_OPERATOR_KERNEL(
    LRN,
    1,
    KernelDefBuilder().TypeConstraint("T", DataTypeImpl::GetTensorType<float>()),
    LRN<float>);

}  // namespace onnxruntime
