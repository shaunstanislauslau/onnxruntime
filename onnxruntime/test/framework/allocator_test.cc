// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/framework/allocatormgr.h"
#include "core/framework/allocator.h"
#include "test_utils.h"
#include "gtest/gtest.h"

namespace onnxruntime {
namespace test {
TEST(AllocatorTest, CPUAllocatorTest) {
  auto cpu_arena = TestCPUExecutionProvider()->GetAllocator(0, OrtMemTypeDefault);

  ASSERT_STREQ(cpu_arena->Info().name, CPU);
  EXPECT_EQ(cpu_arena->Info().id, 0);
  EXPECT_EQ(cpu_arena->Info().type, OrtAllocatorType::OrtArenaAllocator);

  size_t size = 1024;
  auto bytes = cpu_arena->Alloc(size);
  EXPECT_TRUE(bytes);
  //test the bytes are ok for read/write
  memset(bytes, -1, 1024);

  EXPECT_EQ(*((int*)bytes), -1);
  cpu_arena->Free(bytes);
  //todo: test the used / max api.
}

// helper class to validate values in Alloc and Free calls made via IAllocator::MakeUniquePtr
class TestAllocator : public IAllocator {
 public:
  TestAllocator(size_t expected_size) : expected_size_{expected_size} {}

  void* Alloc(size_t size) override {
    EXPECT_EQ(size, expected_size_);
    // return a pointer to the expected size in the result.
    // this isn't valid as a real allocator would return memory of the correct size,
    // however the unit test won't be using the memory and via this mechanism we can validate
    // that the Free argument matches.
    size_t* result = new size_t(size);
    return result;
  }

  void Free(void* p) override {
    // the IAllocatorUniquePtr should be calling this with the contents of what was returned from the Alloc
    size_t* p_sizet = (size_t*)p;
    EXPECT_EQ(*p_sizet, expected_size_);
    delete p_sizet;
  }

  virtual const OrtAllocatorInfo& Info() const override {
    static OrtAllocatorInfo info("test", OrtDeviceAllocator, 0);
    return info;
  }

 private:
  size_t expected_size_;
};

// test that IAllocator::MakeUniquePtr allocates buffers of the expected size
TEST(AllocatorTest, MakeUniquePtrTest) {
  // test float creates buffer of size * sizeof(float)
  size_t num_floats = 16;

  // create allocator that will check the call to Alloc matches the expected size
  auto allocator = std::make_shared<TestAllocator>(num_floats * sizeof(float));
  IAllocatorUniquePtr<float> float_ptr = IAllocator::MakeUniquePtr<float>(allocator, num_floats);
  float_ptr = nullptr;  // reset so TestAllocator.Free is called here

  // void should create buffer of size 16 for void*
  // Create new TestAllocator to validate that.
  allocator = std::make_shared<TestAllocator>(16);
  auto void_ptr = IAllocator::MakeUniquePtr<void>(allocator, 16);
  void_ptr = nullptr;
}
}  // namespace test
}  // namespace onnxruntime
