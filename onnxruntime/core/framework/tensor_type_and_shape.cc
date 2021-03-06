// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/session/onnxruntime_c_api.h"
#include "core/framework/tensor_shape.h"
#include "core/framework/ml_value.h"
#include "core/framework/onnxruntime_typeinfo.h"

#include <assert.h>
#include <stdexcept>
#include <atomic>

using onnxruntime::DataTypeImpl;
using onnxruntime::MLFloat16;
using onnxruntime::Tensor;

struct OrtTensorTypeAndShapeInfo : public onnxruntime::ObjectBase<OrtTensorTypeAndShapeInfo> {
 public:
  friend class onnxruntime::ObjectBase<OrtTensorTypeAndShapeInfo>;

  OrtTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
  onnxruntime::TensorShape shape;

  static OrtTensorTypeAndShapeInfo* Create() {
    return new OrtTensorTypeAndShapeInfo();
  }

  OrtTensorTypeAndShapeInfo(const OrtTensorTypeAndShapeInfo& other) = delete;
  OrtTensorTypeAndShapeInfo& operator=(const OrtTensorTypeAndShapeInfo& other) = delete;

 private:
  OrtTensorTypeAndShapeInfo() = default;
  ~OrtTensorTypeAndShapeInfo() {
    assert(ref_count == 0);
  }
};

#define API_IMPL_BEGIN try {
#define API_IMPL_END                                           \
  }                                                            \
  catch (std::exception & ex) {                                \
    return CreateONNXStatus(ORT_RUNTIME_EXCEPTION, ex.what()); \
  }

ORT_API(OrtTensorTypeAndShapeInfo*, OrtCreateTensorTypeAndShapeInfo) {
  return OrtTensorTypeAndShapeInfo::Create();
}

ORT_API_STATUS_IMPL(OrtSetTensorElementType, _In_ OrtTensorTypeAndShapeInfo* this_ptr, enum OrtTensorElementDataType type) {
  API_IMPL_BEGIN
  this_ptr->type = type;
  return nullptr;
  API_IMPL_END
}

ORT_API_STATUS_IMPL(OrtSetDims, _In_ OrtTensorTypeAndShapeInfo* this_ptr, _In_ const int64_t* dim_values, size_t dim_count) {
  API_IMPL_BEGIN
  this_ptr->shape = onnxruntime::TensorShape(dim_values, dim_count);
  return nullptr;
  API_IMPL_END
}

ORT_API(enum OrtTensorElementDataType, OrtGetTensorElementType, _In_ const struct OrtTensorTypeAndShapeInfo* info) {
  return info->type;
}

ORT_API(size_t, OrtGetNumOfDimensions, _In_ const struct OrtTensorTypeAndShapeInfo* info) {
  return info->shape.NumDimensions();
}

ORT_API(void, OrtGetDimensions, _In_ const struct OrtTensorTypeAndShapeInfo* info, _Out_ int64_t* dim_values, size_t dim_values_length) {
  info->shape.CopyDims(dim_values, dim_values_length);
}

ORT_API(int64_t, OrtGetTensorShapeElementCount, _In_ const OrtTensorTypeAndShapeInfo* this_ptr) {
  return this_ptr->shape.Size();
}

struct ONNXValue;

namespace {
inline OrtTensorElementDataType MLDataTypeToOnnxRuntimeTensorElementDataType(
    const onnxruntime::DataTypeImpl* cpp_type) {
  OrtTensorElementDataType type;
  if (cpp_type == onnxruntime::DataTypeImpl::GetType<float>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<uint8_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<int8_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<uint16_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<int16_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<int32_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<int64_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<std::string>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<bool>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<MLFloat16>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<double>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<uint32_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
  } else if (cpp_type == onnxruntime::DataTypeImpl::GetType<uint64_t>()) {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
  } else {
    type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
  }
  return type;
}
}  // namespace

ONNXStatus* GetTensorShapeAndType(const onnxruntime::TensorShape* shape, const onnxruntime::DataTypeImpl* tensor_data_type, OrtTensorTypeAndShapeInfo** out) {
  OrtTensorElementDataType type = MLDataTypeToOnnxRuntimeTensorElementDataType(tensor_data_type);
  if (ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED == type) {
    return CreateONNXStatus(ORT_FAIL, "Not implemented");
  }
  OrtTensorTypeAndShapeInfo* ret = OrtCreateTensorTypeAndShapeInfo();
  auto status = OrtSetTensorElementType(ret, type);
  if (status != nullptr) {
    OrtReleaseObject(ret);
    return status;
  }
  if (shape != nullptr) {
    status = OrtSetDims(ret, shape->GetDims().data(), shape->GetDims().size());
    if (status != nullptr) {
      OrtReleaseObject(ret);
      return status;
    }
  }
  *out = ret;
  return nullptr;
}

ORT_API_STATUS_IMPL(OrtGetTensorShapeAndType, _In_ const ONNXValue* value,
                    _Out_ OrtTensorTypeAndShapeInfo** out) {
  API_IMPL_BEGIN
  auto v = reinterpret_cast<const ::onnxruntime::MLValue*>(value);
  const onnxruntime::Tensor& tensor = v->Get<onnxruntime::Tensor>();
  return GetTensorShapeAndType(&tensor.Shape(), tensor.DataType(), out);
  API_IMPL_END
}

ORT_API(enum OrtType, OrtGetValueType, _In_ const ONNXValue* value) {
  try {
    auto v = reinterpret_cast<const ::onnxruntime::MLValue*>(value);
    onnxruntime::MLDataType type = v->Type();
    OrtTypeInfo* out;
    ONNXStatus* ptr = OrtTypeInfo::FromDataTypeImpl(type, nullptr, nullptr, &out);
    if (ptr != nullptr) {
      ReleaseONNXStatus(ptr);
      return ORT_TYPE_UNKNOWN;
    }
    OrtType ret = out->type;
    OrtReleaseObject(out);
    return ret;
  } catch (std::exception&) {
    return ORT_TYPE_UNKNOWN;
  }
}

/**
 * Get the type information of an ONNXValue
 * \param value
 * \return The returned value should be freed by OrtReleaseObject after use
 */
ORT_API_STATUS_IMPL(OrtGetTypeInfo, _In_ const ONNXValue* value, struct OrtTypeInfo** out) {
  auto v = reinterpret_cast<const ::onnxruntime::MLValue*>(value);
  onnxruntime::MLDataType type = v->Type();
  if (type == nullptr) {
    *out = nullptr;
    return nullptr;
  }
  if (type == DataTypeImpl::GetType<Tensor>()) {
    const onnxruntime::Tensor& tensor = v->Get<onnxruntime::Tensor>();
    const onnxruntime::TensorShape& shape = tensor.Shape();
    return OrtTypeInfo::FromDataTypeImpl(type, &shape, tensor.DataType(), out);
  }
  return OrtTypeInfo::FromDataTypeImpl(type, nullptr, nullptr, out);
}
