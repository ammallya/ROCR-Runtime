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

#include <string>
#include <vector>
#include <memory>

#include "gtest/gtest.h"
#include "suites/performance/dispatch_time.h"
#include "suites/performance/memory_async_copy.h"
#include "suites/performance/test_case_template.h"
#include "suites/performance/main.h"
#include "suites/test_common/test_common.h"

#include "common/rocm_smi/rocm_smi.h"


static RocrTstGlobals *sRocrtstGlvalues = nullptr;

static bool GetMonitorDevices(const std::shared_ptr<rocrtst::smi::Device> &d,
                                                                    void *p) {
  std::string val_str;

  assert(p != nullptr);

  std::vector<std::shared_ptr<rocrtst::smi::Device>> *device_list =
    reinterpret_cast<std::vector<std::shared_ptr<rocrtst::smi::Device>> *>(p);

  if (d->monitor() != nullptr) {
    device_list->push_back(d);
  }
  return false;
}


static void RunTest(TestBase *test) {
  assert(sRocrtstGlvalues != nullptr);

  test->set_verbosity(sRocrtstGlvalues->verbosity);
  test->set_monitor_verbosity(sRocrtstGlvalues->monitor_verbosity);
  test->set_num_iteration(sRocrtstGlvalues->num_iterations);
  test->set_monitor_devices(&sRocrtstGlvalues->monitor_devices);

  test->DisplayTestInfo();
  test->SetUp();
  test->Run();
  test->DisplayResults();
  test->Close();

  return;
}

// TEST ENTRY TEMPLATE:
// TEST(rocrtst, Perf_<test name>) {
//  <Test Implementation class> <test_obj>;
//
//  // Copy and modify implementation of RunTest() if you need to deviate
//  // from the standard pattern implemented there.
//  RunTest(&<test_obj>);
// }

TEST(rocrtst, Test_Example) {
  TestExample tst;

  rocrtst::smi::RocmSMI hw;
  hw.DiscoverDevices();

  RunTest(&tst);
}

TEST(rocrtst, Perf_Memory_Async_Copy) {
  MemoryAsyncCopy mac;
  // To do full test, uncomment this:
  //  mac.set_full_test(true);
  // To test only 1 path, add lines like this:
  //  mac.set_src_pool(<src pool id>);
  //  mac.set_dst_pool(<dst pool id>);
  // The default is to and from the cpu to 1 gpu, and to/from a gpu to
  // another gpu
  RunTest(&mac);
}

TEST(rocrtst, Perf_Dispatch_Time_Single_SpinWait) {
  DispatchTime dt(true, true);
  RunTest(&dt);
}

TEST(rocrtst, Perf_Dispatch_Time_Single_Interrupt) {
  DispatchTime dt(false, true);
  RunTest(&dt);
}

TEST(rocrtst, Perf_Dispatch_Time_Multi_SpinWait) {
  DispatchTime dt(true, false);
  RunTest(&dt);
}

TEST(rocrtst, Perf_Dispatch_Time_Multi_Interrupt) {
  DispatchTime dt(false, false);
  RunTest(&dt);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  RocrTstGlobals settings;

  settings.verbosity = 1;
  settings.monitor_verbosity = 1;
  settings.num_iterations = 0;

  if (ProcessCmdline(&settings, argc, argv)) {
    return 1;
  }

  rocrtst::smi::RocmSMI hw;
  hw.DiscoverDevices();
  hw.IterateSMIDevices(
       GetMonitorDevices, reinterpret_cast<void *>(&settings.monitor_devices));

  sRocrtstGlvalues = &settings;

  return RUN_ALL_TESTS();
}
