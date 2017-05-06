/*
 * =============================================================================
 *   ROC Runtime Conformance Release License
 * =============================================================================
 * The University of Illinois/NCSA
 * Open Source License (NCSA)
 *
 * Copyright (c) 2017, Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Developed by:
 *
 *                 AMD Research and AMD ROC Software Development
 *
 *                 Advanced Micro Devices, Inc.
 *
 *                 www.amd.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in
 *    the documentation and/or other materials provided with the distribution.
 *  - Neither the names of <Name of Development Group, Name of Institution>,
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this Software without specific prior written
 *    permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include "device_store_bandwidth.h"
#include "common/base_rocr_utils.h"
#include "common/common.h"
#include "common/helper_funcs.h"
#include "common/hsatimer.h"
#include "gtest/gtest.h"

// Constructor
DeviceStoreBandwidth::DeviceStoreBandwidth() :
  BaseRocR() {

  set_group_size(0);
  num_group_ = 0;
  num_cus_ = 0;

  kernel_loop_count_ = 0;
  mean_ = 0.0;
  data_size_ = 0;
  set_requires_profile (HSA_PROFILE_BASE);
  in_data_ = nullptr;
  out_data_ = nullptr;
}

// Destructor
DeviceStoreBandwidth::~DeviceStoreBandwidth() {
}

// Set up the test environment
void DeviceStoreBandwidth::SetUp() {
  SetWorkItemNum();

  set_kernel_file_name("sysMemWrite.o");
  set_kernel_name("&__SysMemStore");

  if (HSA_STATUS_SUCCESS != rocrtst::InitAndSetupHSA(this)) {
    return;
  }

  hsa_agent_t* gpu_dev = gpu_device1();

  //Create a queue with max number size
  hsa_queue_t* q = nullptr;
  rocrtst::CreateQueue(*gpu_dev, &q);
  ASSERT_NE(q, nullptr);
  set_main_queue(q);

  rocrtst::LoadKernelFromObjFile(this);

  uint32_t total_work_items = num_cus_ * num_group_ * group_size();

  //Fill up part of aql
  rocrtst::InitializeAQLPacket(this, &aql());
  aql().workgroup_size_x = group_size();
  aql().grid_size_x = total_work_items;

  return;
}

// Run the test
void DeviceStoreBandwidth::Run() {
  hsa_status_t err;

  if (!rocrtst::CheckProfile(this)) {
    return;
  }

  uint32_t total_workitems = num_cus_ * num_group_ * group_size();

  uint32_t ops_thrd = 16;
  uint64_t addr_step = (uint64_t) total_workitems * sizeof(uint32_t);
  uint64_t total_ops = (uint64_t) total_workitems * kernel_loop_count_
                       * ops_thrd;
  uint64_t in_data_size = (uint64_t) total_ops * sizeof(uint32_t);

  data_size_ = in_data_size;

  err = rocrtst::SetPoolsTypical(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  err = rocrtst::AllocAndAllowAccess(this, in_data_size, device_pool(),
                                                  (void**)&in_data_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  uint32_t out_data_size = total_workitems * sizeof(uint32_t);

  err = rocrtst::AllocAndAllowAccess(this, out_data_size, device_pool(),
                                                          (void**)&out_data_);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  struct local_args_t {
    void* arg0;
    void* arg1;
    uint64_t arg2;
    void* arg3;
  } local_args;

  local_args.arg0 = in_data_;
  local_args.arg1 = in_data_ + total_ops;
  local_args.arg2 = addr_step;
  local_args.arg3 = out_data_;

  // Copy the kernel args structure into a registered memory block
  err = rocrtst::AllocAndSetKernArgs(this, &local_args, sizeof(local_args));
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);

  std::vector<double> time;

  rocrtst::WriteAQLToQueue(this);

  for (uint32_t i = 0; i < num_iteration(); i++) {
    uint64_t que_idx = hsa_queue_load_write_index_relaxed(main_queue());

    // Write the aql packet at the calculated queue index address.
    const uint32_t queue_mask = main_queue()->size - 1;

    rocrtst::PerfTimer p_timer;
    int id = p_timer.CreateTimer();
    p_timer.StartTimer(id);

    void * q_base = main_queue()->base_address;
    uint32_t aql_header = HSA_PACKET_TYPE_KERNEL_DISPATCH;
    rocrtst::AtomicSetPacketHeader(aql_header, aql().setup,
             &((hsa_kernel_dispatch_packet_t*)(q_base))[que_idx & queue_mask]);
    hsa_signal_store_screlease(main_queue()->doorbell_signal, que_idx);

    // Wait on the dispatch signal until the kernel is finished.
    while (hsa_signal_wait_scacquire(signal(), HSA_SIGNAL_CONDITION_LT, 1,
                                     (uint64_t) - 1, HSA_WAIT_STATE_ACTIVE))
      ;

    p_timer.StopTimer(id);

#ifdef DEBUG
    std::cout << "." << std::flush;
#endif

    time.push_back(p_timer.ReadTimer(id));

    hsa_signal_store_screlease(signal(), 1);
  }

#ifdef DEBUG
  std::cout << std::endl;
#endif

  time.erase(time.begin());
  mean_ = rocrtst::CalcMean(time);

  return;
}

void DeviceStoreBandwidth::Close() {
  hsa_status_t err;

  err = hsa_amd_memory_pool_free(in_data_);
  EXPECT_EQ(err, HSA_STATUS_SUCCESS);
  err = hsa_amd_memory_pool_free(out_data_);
  EXPECT_EQ(err, HSA_STATUS_SUCCESS);

  err = rocrtst::CommonCleanUp(this);
  ASSERT_EQ(err, HSA_STATUS_SUCCESS);
  return;
}

void DeviceStoreBandwidth::DisplayResults() const {
  if (!rocrtst::CheckProfile(this)) {
    return;
  }
  std::cout << "=======================================" << std::endl;
  std::cout << "Device Store Bandwidth:     ";
  std::cout << data_size_ / mean_ / 1024 / 1024 / 1024 << "(GB/S)" << std::endl;
  std::cout << "=======================================" << std::endl;
  return;
}
