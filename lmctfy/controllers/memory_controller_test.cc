// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lmctfy/controllers/memory_controller.h"

#include <memory>

#include "base/integral_types.h"
#include "system_api/kernel_api_mock.h"
#include "file/base/path.h"
#include "lmctfy/controllers/eventfd_notifications_mock.h"
#include "lmctfy/kernel_files.h"
#include "util/safe_types/bytes.h"
#include "util/errors_test_util.h"
#include "strings/stringpiece.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/task/codes.pb.h"
#include "util/task/status.h"

using ::system_api::KernelAPIMock;
using ::file::JoinPath;
using ::util::Bytes;
using ::std::map;
using ::std::unique_ptr;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;
using ::util::Status;
using ::util::StatusOr;
using ::util::error::NOT_FOUND;

namespace containers {
namespace lmctfy {

static const char kMountPoint[] = "/dev/cgroup/memory/test";

class MemoryControllerTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    mock_kernel_.reset(new StrictMock<KernelAPIMock>());
    mock_eventfd_notifications_.reset(MockEventFdNotifications::NewStrict());
    controller_.reset(new MemoryController(kMountPoint, true,
                                           mock_kernel_.get(),
                                           mock_eventfd_notifications_.get()));
  }

  StatusOr<map<string, int64>> CallGetStats(const string &stats_type) const {
    return controller_->GetStats(stats_type);
  }

  StatusOr<ActiveNotifications::Handle> CallRegisterOomNotification(
      CgroupController::EventCallback *cb) {
    return controller_->RegisterOomNotification(cb);
  }

  StatusOr<ActiveNotifications::Handle> CallRegisterUsageThresholdNotification(
      Bytes usage_threshold, CgroupController::EventCallback *cb) {
    return controller_->RegisterUsageThresholdNotification(usage_threshold, cb);
  }

 protected:
  unique_ptr<KernelAPIMock> mock_kernel_;
  unique_ptr<MemoryController> controller_;
  unique_ptr<MockEventFdNotifications> mock_eventfd_notifications_;
};

TEST_F(MemoryControllerTest, Type) {
  EXPECT_EQ(CGROUP_MEMORY, controller_->type());
}

TEST_F(MemoryControllerTest, SetLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_TRUE(controller_->SetLimit(Bytes(42)).ok());
}

TEST_F(MemoryControllerTest, SetInfiniteLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("-1", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_TRUE(controller_->SetLimit(Bytes(kint64max)).ok());
}

TEST_F(MemoryControllerTest, SetLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_FALSE(controller_->SetLimit(Bytes(42)).ok());
}

TEST_F(MemoryControllerTest, SetSoftLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_TRUE(controller_->SetSoftLimit(Bytes(42)).ok());
}

TEST_F(MemoryControllerTest, SetInfiniteSoftLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("-1", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_TRUE(controller_->SetSoftLimit(Bytes(kint64max)).ok());
}

TEST_F(MemoryControllerTest, SetSoftLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_FALSE(controller_->SetSoftLimit(Bytes(42)).ok());
}

TEST_F(MemoryControllerTest, SetSwapLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_OK(controller_->SetSwapLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetInfiniteSwapLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("-1", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_OK(controller_->SetSwapLimit(Bytes(kint64max)));
}

TEST_F(MemoryControllerTest, SetSwapLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_NOT_OK(controller_->SetSwapLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetStalePageAge) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStalePageAge);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_OK(controller_->SetStalePageAge(42));
}

TEST_F(MemoryControllerTest, SetStalePageAgeFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStalePageAge);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_NOT_OK(controller_->SetStalePageAge(42));
}

TEST_F(MemoryControllerTest, SetOomScore) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kOomScoreBadness);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_OK(controller_->SetOomScore(42));
}

TEST_F(MemoryControllerTest, SetOomScoreFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kOomScoreBadness);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_NOT_OK(controller_->SetOomScore(42));
}

TEST_F(MemoryControllerTest, SetCompressionSamplingRatio) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kCompressionSamplingRatio);

  EXPECT_CALL(*mock_kernel_, SafeWriteResFile("42", kResFile, NotNull(),
                                              NotNull())).WillOnce(Return(0));

  EXPECT_OK(controller_->SetCompressionSamplingRatio(42));
}

TEST_F(MemoryControllerTest, SetCompressionSamplingRatioFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kCompressionSamplingRatio);

  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));

  EXPECT_NOT_OK(controller_->SetCompressionSamplingRatio(42));
}

TEST_F(MemoryControllerTest, SetDirtyRatioSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyRatio);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(Return(0));
  EXPECT_OK(controller_->SetDirtyRatio(42));
}

TEST_F(MemoryControllerTest, SetDirtyRatioFailure) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyRatio);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));
  EXPECT_NOT_OK(controller_->SetDirtyRatio(42));
}

TEST_F(MemoryControllerTest, SetDirtyBackgroundRatioSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundRatio);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(Return(0));
  EXPECT_OK(controller_->SetDirtyBackgroundRatio(42));
}

TEST_F(MemoryControllerTest, SetDirtyBackgroundRatioFailure) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundRatio);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));
  EXPECT_NOT_OK(controller_->SetDirtyBackgroundRatio(42));
}

TEST_F(MemoryControllerTest, SetDirtyLimitSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyLimitInBytes);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(Return(0));
  EXPECT_OK(controller_->SetDirtyLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetDirtyLimitFailure) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyLimitInBytes);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));
  EXPECT_NOT_OK(controller_->SetDirtyLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetDirtyBackgroundLimitSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundLimitInBytes);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(Return(0));
  EXPECT_OK(controller_->SetDirtyBackgroundLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetDirtyBackgroundLimitFailure) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundLimitInBytes);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("42", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));
  EXPECT_NOT_OK(controller_->SetDirtyBackgroundLimit(Bytes(42)));
}

TEST_F(MemoryControllerTest, SetKMemChargeUsageSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("0", kResFile, NotNull(), NotNull()))
      .WillOnce(Return(0));
  EXPECT_OK(controller_->SetKMemChargeUsage(false));
}

TEST_F(MemoryControllerTest, SetKMemChargeUsageFailure) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);
  EXPECT_CALL(*mock_kernel_,
              SafeWriteResFile("0", kResFile, NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<3>(true), Return(0)));
  EXPECT_NOT_OK(controller_->SetKMemChargeUsage(false));
}

TEST_F(MemoryControllerTest, GetLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetLimit();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetLimit());
}

TEST_F(MemoryControllerTest, GetLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetLimit().ok());
}

TEST_F(MemoryControllerTest, GetSoftLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetSoftLimit();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetSoftLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetSoftLimit());
}

TEST_F(MemoryControllerTest, GetSoftLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kSoftLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetSoftLimit().ok());
}

TEST_F(MemoryControllerTest, GetSwapLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetSwapLimit();
  ASSERT_OK(statusor);
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetSwapLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetSwapLimit());
}

TEST_F(MemoryControllerTest, GetSwapLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetSwapLimit());
}

TEST_F(MemoryControllerTest, GetStalePageAge) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStalePageAge);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<unsigned int> statusor = controller_->GetStalePageAge();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetStalePageAgeNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStalePageAge);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetStalePageAge());
}

TEST_F(MemoryControllerTest, GetStalePageAgeFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStalePageAge);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetStalePageAge());
}

TEST_F(MemoryControllerTest, GetOomScore) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kOomScoreBadness);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<unsigned int> statusor = controller_->GetOomScore();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetOomScoreNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kOomScoreBadness);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetOomScore());
}

TEST_F(MemoryControllerTest, GetOomScoreFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kOomScoreBadness);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetOomScore());
}

TEST_F(MemoryControllerTest, GetCompressionSamplingRatio) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kCompressionSamplingRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<unsigned int> statusor = controller_->GetCompressionSamplingRatio();
  ASSERT_OK(statusor);
  EXPECT_EQ(42, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetCompressionSamplingRatioNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kCompressionSamplingRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetCompressionSamplingRatio());
}

TEST_F(MemoryControllerTest, GetCompressionSamplingRatioFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kCompressionSamplingRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetCompressionSamplingRatio());
}

TEST_F(MemoryControllerTest, GetUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetUsage();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetUsageNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetUsage());
}

TEST_F(MemoryControllerTest, GetUsageFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetUsage().ok());
}

TEST_F(MemoryControllerTest, GetMaxUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetMaxUsage();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetMaxUsageNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetMaxUsage());
}

TEST_F(MemoryControllerTest, GetMaxUsageFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetMaxUsage().ok());
}

TEST_F(MemoryControllerTest, GetSwapUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetSwapUsage();
  ASSERT_OK(statusor);
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetSwapUsageNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetSwapUsage());
}

TEST_F(MemoryControllerTest, GetSwapUsageFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetSwapUsage());
}

TEST_F(MemoryControllerTest, GetSwapMaxUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetSwapMaxUsage();
  ASSERT_OK(statusor);
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetSwapMaxUsageNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetSwapMaxUsage());
}

TEST_F(MemoryControllerTest, GetSwapMaxUsageFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::Memsw::kMaxUsageInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetSwapMaxUsage());
}

TEST_F(MemoryControllerTest, GetDirtyRatio) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyRatio);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<int32> statusor = controller_->GetDirtyRatio();
  EXPECT_OK(statusor);
  EXPECT_EQ(42, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetDirtyRatioNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetDirtyRatio());
}

TEST_F(MemoryControllerTest, GetDirtyRatioFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetDirtyRatio().ok());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundRatio) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundRatio);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<int32> statusor = controller_->GetDirtyBackgroundRatio();
  EXPECT_OK(statusor);
  EXPECT_EQ(42, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundRatioNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetDirtyBackgroundRatio());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundRatioFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundRatio);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetDirtyBackgroundRatio().ok());
}

TEST_F(MemoryControllerTest, GetDirtyLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyLimitInBytes);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetDirtyLimit();
  EXPECT_OK(statusor);
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetDirtyLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetDirtyLimit());
}

TEST_F(MemoryControllerTest, GetDirtyLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetDirtyLimit().ok());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundLimitInBytes);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetDirtyBackgroundLimit();
  EXPECT_OK(statusor);
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetDirtyBackgroundLimit());
}

TEST_F(MemoryControllerTest, GetDirtyBackgroundLimitFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kDirtyBackgroundLimitInBytes);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetDirtyBackgroundLimit().ok());
}

TEST_F(MemoryControllerTest, GetKMemChargeUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("1"), Return(true)));

  StatusOr<bool> statusor = controller_->GetKMemChargeUsage();
  ASSERT_OK(statusor);
  EXPECT_TRUE(statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetKMemChargeUsageBadKernelValue) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("2"), Return(true)));

  EXPECT_ERROR_CODE(::util::error::OUT_OF_RANGE,
                    controller_->GetKMemChargeUsage());
}

TEST_F(MemoryControllerTest, GetKMemChargeUsageNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetKMemChargeUsage());
}

TEST_F(MemoryControllerTest, GetKMemChargeUsageFails) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kKMemChargeUsage);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetKMemChargeUsage());
}


TEST_F(MemoryControllerTest, GetWorkingSet) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStats =
      "idle_120_dirty_file 0\n"
      "idle_120_dirty_swap 0\n"
      "idle_240_clean 0\n"
      "idle_240_dirty_file 0\n"
      "idle_240_dirty_swap 0\n"
      "scans 30\n"
      "stale 20\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetWorkingSet();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(22), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetWorkingSetMoreStaleThanUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStats =
      "idle_120_dirty_file 0\n"
      "idle_120_dirty_swap 0\n"
      "idle_240_clean 0\n"
      "idle_240_dirty_file 0\n"
      "idle_240_dirty_swap 0\n"
      "scans 30\n"
      "stale 52\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetWorkingSet();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(0), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetWorkingSetFailsToReadStats) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetWorkingSet().ok());
}

TEST_F(MemoryControllerTest, GetWorkingSetFailsToReadUsage) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStats =
      "idle_120_dirty_file 0\n"
      "idle_120_dirty_swap 0\n"
      "idle_240_clean 0\n"
      "idle_240_dirty_file 0\n"
      "idle_240_dirty_swap 0\n"
      "scans 30\n"
      "stale 52\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_FALSE(controller_->GetWorkingSet().ok());
}

TEST_F(MemoryControllerTest, GetWorkingSetUsageFileNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStats =
      "idle_120_dirty_file 0\n"
      "idle_120_dirty_swap 0\n"
      "idle_240_clean 0\n"
      "idle_240_dirty_file 0\n"
      "idle_240_dirty_swap 0\n"
      "scans 30\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetWorkingSet());
}

TEST_F(MemoryControllerTest, GetWorkingSetIdlePageFileNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kIdleFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "cache 30806016\n"
      "rss 770048\n"
      "total_inactive_anon 14\n"
      "total_active_anon 287440896\n"
      "total_inactive_file 10\n"
      "total_active_file 14071710\n"
      "total_unevictable 24645632\n"
      "scans 30\n";
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kIdleFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>("1024"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kStats), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetWorkingSet();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(1000), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetWorkingSetInactiveNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kIdleFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "cache 30806016\n"
      "writeback 0\n"
      "total_inactive_anon 14\n"
      "total_active_anon 287440896\n"
      "total_active_file 14071710\n"
      "total_unevictable 24645632\n"
      "scans 30\n";
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kIdleFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>("1024"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_NOT_OK(controller_->GetWorkingSet());
}

TEST_F(MemoryControllerTest, GetWorkingSetStatFileNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kIdleFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kIdleFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>("1024"), Return(true)));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetWorkingSet());
}

TEST_F(MemoryControllerTest, GetWorkingSetFailToReadInactive) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kIdleFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kIdleFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>("1024"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_NOT_OK(controller_->GetWorkingSet());
}

TEST_F(MemoryControllerTest, GetWorkingSetNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kUsageInBytes);
  const string kIdleFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kIdlePageStats);
  const string kStatFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kIdleStats =
      "idle_120_dirty_file 0\n"
      "idle_120_dirty_swap 0\n"
      "idle_240_clean 0\n"
      "idle_240_dirty_file 0\n"
      "idle_240_dirty_swap 0\n"
      "scans 30\n";
  // Inactive file stat missing.
  const string kStats =
      "cache 30806016\n"
      "writeback 0\n"
      "total_inactive_anon 14\n"
      "total_active_anon 287440896\n"
      "total_active_file 14071710\n"
      "total_unevictable 24645632\n"
      "scans 30\n";
  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kIdleFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, Access(kStatFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("42"), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kIdleFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kIdleStats), Return(true)));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kStatFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_NOT_OK(controller_->GetWorkingSet());
}

TEST_F(MemoryControllerTest, GetEffectiveLimit) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "kernel_noncharged_targeted_slab_memory 0\n"
      "thp_fault_alloc 11776\n"
      "thp_fault_fallback 0\n"
      "thp_collapse_alloc 1536\n"
      "thp_collapse_alloc_failed 0\n"
      "thp_split 1536\n"
      "hierarchical_memory_limit 42\n"
      "total_cache 397692928\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  StatusOr<Bytes> statusor = controller_->GetEffectiveLimit();
  ASSERT_TRUE(statusor.ok());
  EXPECT_EQ(Bytes(42), statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, GetEffectiveLimitFileNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "kernel_noncharged_targeted_slab_memory 0\n"
      "thp_fault_alloc 11776\n"
      "thp_fault_fallback 0\n"
      "thp_collapse_alloc 1536\n"
      "thp_collapse_alloc_failed 0\n"
      "thp_split 1536\n"
      "total_cache 397692928\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_ERROR_CODE(NOT_FOUND, controller_->GetEffectiveLimit());
}

TEST_F(MemoryControllerTest, GetEffectiveLimitNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "kernel_noncharged_targeted_slab_memory 0\n"
      "thp_fault_alloc 11776\n"
      "thp_fault_fallback 0\n"
      "thp_collapse_alloc 1536\n"
      "thp_collapse_alloc_failed 0\n"
      "thp_split 1536\n"
      "total_cache 397692928\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_FALSE(controller_->GetEffectiveLimit().ok());
}

TEST_F(MemoryControllerTest, GetMemoryStatsSuccess) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "kernel_noncharged_targeted_slab_memory 0\n"
      "thp_fault_alloc 11776\n"
      "thp_fault_fallback 0\n"
      "thp_collapse_alloc 1536\n"
      "thp_collapse_alloc_failed 0\n"
      "thp_split 1536\n"
      "total_cache 397692928\n"
      "hierarchical_memory_limit 1234\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  MemoryStats stats;
  Status status = controller_->GetMemoryStats(&stats);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(1536, stats.container_data().thp().split());
  EXPECT_FALSE(stats.container_data().has_cache());
  EXPECT_EQ(397692928, stats.hierarchical_data().cache());
  EXPECT_EQ(1234, stats.hierarchical_memory_limit());
}

TEST_F(MemoryControllerTest, GetMemoryStatsFileNotFound) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "kernel_noncharged_targeted_slab_memory 0\n"
      "thp_fault_alloc 11776\n"
      "thp_fault_fallback 0\n"
      "thp_collapse_alloc 1536\n"
      "thp_collapse_alloc_failed 0\n"
      "thp_split 1536\n"
      "total_cache 397692928\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(1));

  MemoryStats stats;
  EXPECT_FALSE(controller_->GetMemoryStats(&stats).ok());
}

TEST_F(MemoryControllerTest, GetEffectiveLimitFailsToReadStats) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(controller_->GetEffectiveLimit().ok());
}

TEST_F(MemoryControllerTest, CallGetStats) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "a1 1\n"
      "a2 2\n"
      "a3 3\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  StatusOr<map<string, int64>> statusor =
      CallGetStats(KernelFiles::Memory::kStat);
  ASSERT_TRUE(statusor.ok());
  map<string, int64> output = statusor.ValueOrDie();
  ASSERT_EQ(3, output.size());
  EXPECT_EQ(1, output["a1"]);
  EXPECT_EQ(2, output["a2"]);
  EXPECT_EQ(3, output["a3"]);
}

TEST_F(MemoryControllerTest, CallGetStatsBadLineFormat) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "a1 1\n"
      "a2 2\n"
      "a3 3 4\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_EQ(::util::error::FAILED_PRECONDITION,
            CallGetStats(KernelFiles::Memory::kStat).status().error_code());
}

TEST_F(MemoryControllerTest, CallGetStatsValueNotAnInteger) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);
  const string kStats =
      "a1 1\n"
      "a2 2\n"
      "a3 potato\n";

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(kStats), Return(true)));

  EXPECT_EQ(::util::error::FAILED_PRECONDITION,
            CallGetStats(KernelFiles::Memory::kStat).status().error_code());
}

TEST_F(MemoryControllerTest, CallGetStatsFailsToReadStats) {
  const string kResFile =
      JoinPath(kMountPoint, KernelFiles::Memory::kStat);

  EXPECT_CALL(*mock_kernel_, Access(kResFile, F_OK))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_kernel_, ReadFileToString(kResFile, NotNull()))
      .WillOnce(Return(false));

  EXPECT_FALSE(CallGetStats(KernelFiles::Memory::kStat).ok());
}

// Dummy notification handler for use in testing.
void NoOpCallback(Status status) {
  EXPECT_TRUE(false) << "Should never be called";
}

TEST_F(MemoryControllerTest, RegisterUsageThresholdNotificationSuccess) {
  // Normally RegisterNotification() takes ownership, but we are mocking it out.
  unique_ptr<CgroupController::EventCallback> cb(
      NewPermanentCallback(&NoOpCallback));

  EXPECT_CALL(
      *mock_eventfd_notifications_,
      RegisterNotification(kMountPoint, KernelFiles::Memory::kUsageInBytes,
                           "4096", NotNull())).WillOnce(Return(1));

  StatusOr<ActiveNotifications::Handle> statusor =
      CallRegisterUsageThresholdNotification(Bytes(4096), cb.get());
  ASSERT_OK(statusor);
  EXPECT_EQ(1, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, RegisterUsageThresholdNotificationFails) {
  // Normally RegisterNotification() takes ownership, but we are mocking it out.
  unique_ptr<CgroupController::EventCallback> cb(
      NewPermanentCallback(&NoOpCallback));

  EXPECT_CALL(*mock_eventfd_notifications_,
              RegisterNotification(
                  kMountPoint, KernelFiles::Memory::kUsageInBytes, "4096",
                  NotNull())).WillOnce(Return(Status::CANCELLED));

  EXPECT_ERROR_CODE(
      ::util::error::CANCELLED,
      CallRegisterUsageThresholdNotification(Bytes(4096), cb.get()));
}

TEST_F(MemoryControllerTest, RegisterOomNotificationSuccess) {
  // Normally RegisterNotification() takes ownership, but we are mocking it out.
  unique_ptr<CgroupController::EventCallback> cb(
      NewPermanentCallback(&NoOpCallback));

  EXPECT_CALL(*mock_eventfd_notifications_,
              RegisterNotification(
                  kMountPoint, KernelFiles::Memory::kOomControl, "", NotNull()))
      .WillOnce(Return(1));

  StatusOr<ActiveNotifications::Handle> statusor =
      CallRegisterOomNotification(cb.get());
  ASSERT_OK(statusor);
  EXPECT_EQ(1, statusor.ValueOrDie());
}

TEST_F(MemoryControllerTest, RegisterOomNotificationFails) {
  // Normally RegisterNotification() takes ownership, but we are mocking it out.
  unique_ptr<CgroupController::EventCallback> cb(
      NewPermanentCallback(&NoOpCallback));

  EXPECT_CALL(*mock_eventfd_notifications_,
              RegisterNotification(
                  kMountPoint, KernelFiles::Memory::kOomControl, "", NotNull()))
      .WillOnce(Return(Status::CANCELLED));

  EXPECT_ERROR_CODE(::util::error::CANCELLED,
                    CallRegisterOomNotification(cb.get()));
}

}  // namespace lmctfy
}  // namespace containers
