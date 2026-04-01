/******************************************************************************
 * Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#include <cstdlib>
#include "../src/gda/ibv_wrapper.hpp"
#include "topology_gtest.hpp"

using namespace rocshmem;

namespace {

// Same device order as BuildFilteredNicAddresses / GetNumDevices(EXE_NIC) (ibv_get_device_list).
std::string IbvDeviceNameAtIndex(int nicIndex) {
  int num = 0;
  struct ibv_device** list = ibv.get_device_list(&num);
  if (!list || nicIndex < 0 || nicIndex >= num) {
    if (list) ibv.free_device_list(list);
    return {};
  }
  const char* n = ibv.get_device_name(list[nicIndex]);
  std::string out = n ? n : "";
  ibv.free_device_list(list);
  return out;
}

// ibverbs-style device names for NICs with active ports (same source as topology).
bool CollectActiveNicNamesForGpu0(std::vector<std::string>* out_names) {
  out_names->clear();
  if (GetNumDevices(EXE_GPU) == 0 || GetNumDevices(EXE_NIC) == 0) return false;
  int n = GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, *out_names);
  return n > 0 && !out_names->empty();
}

// If the list contains two different strings, sets *a and *b and returns true.
bool TwoDistinctNicNames(const std::vector<std::string>& names, std::string* a,
                         std::string* b) {
  a->clear();
  b->clear();
  if (names.empty()) return false;
  *a = names[0];
  for (size_t i = 1; i < names.size(); ++i) {
    if (names[i] != *a) {
      *b = names[i];
      return true;
    }
  }
  return false;
}

}  // namespace

// Test DeviceType helper functions
TEST_F(DeviceTypeTestFixture, IsCpuExeType) {
  EXPECT_TRUE(IsCpuExeType(EXE_CPU));
  EXPECT_FALSE(IsCpuExeType(EXE_GPU));
  EXPECT_FALSE(IsCpuExeType(EXE_NIC));
}

TEST_F(DeviceTypeTestFixture, IsGpuExeType) {
  EXPECT_FALSE(IsGpuExeType(EXE_CPU));
  EXPECT_TRUE(IsGpuExeType(EXE_GPU));
  EXPECT_FALSE(IsGpuExeType(EXE_NIC));
}

TEST_F(DeviceTypeTestFixture, IsNicExeType) {
  EXPECT_FALSE(IsNicExeType(EXE_CPU));
  EXPECT_FALSE(IsNicExeType(EXE_GPU));
  EXPECT_TRUE(IsNicExeType(EXE_NIC));
}

// Test MemType helper functions
TEST_F(DeviceTypeTestFixture, IsCpuMemType) {
  EXPECT_TRUE(IsCpuMemType(MEM_CPU));
  EXPECT_FALSE(IsCpuMemType(MEM_GPU));
}

TEST_F(DeviceTypeTestFixture, IsGpuMemType) {
  EXPECT_FALSE(IsGpuMemType(MEM_CPU));
  EXPECT_TRUE(IsGpuMemType(MEM_GPU));
}

// Test ExeDevice struct
TEST_F(DeviceTypeTestFixture, ExeDeviceComparison) {
  ExeDevice cpu0 = {EXE_CPU, 0};
  ExeDevice cpu1 = {EXE_CPU, 1};
  ExeDevice gpu0 = {EXE_GPU, 0};

  // Test less-than operator
  EXPECT_TRUE(cpu0 < cpu1);   // Same type, different index
  EXPECT_TRUE(cpu0 < gpu0);   // Different type (CPU < GPU)
  EXPECT_FALSE(cpu1 < cpu0);  // Reverse comparison
}

TEST_F(DeviceTypeTestFixture, ExeDeviceEquality) {
  ExeDevice cpu0_a = {EXE_CPU, 0};
  ExeDevice cpu0_b = {EXE_CPU, 0};

  // Two identical devices should not be less than each other
  EXPECT_FALSE(cpu0_a < cpu0_b);
  EXPECT_FALSE(cpu0_b < cpu0_a);
}

// Test MemDevice struct
TEST_F(DeviceTypeTestFixture, MemDeviceComparison) {
  MemDevice cpuMem0 = {MEM_CPU, 0};
  MemDevice cpuMem1 = {MEM_CPU, 1};
  MemDevice gpuMem0 = {MEM_GPU, 0};

  // Test less-than operator
  EXPECT_TRUE(cpuMem0 < cpuMem1);   // Same type, different index
  EXPECT_TRUE(cpuMem0 < gpuMem0);   // Different type (MEM_CPU < MEM_GPU)
  EXPECT_FALSE(cpuMem1 < cpuMem0);  // Reverse comparison
}

TEST_F(DeviceTypeTestFixture, MemDeviceEquality) {
  MemDevice cpuMem0_a = {MEM_CPU, 0};
  MemDevice cpuMem0_b = {MEM_CPU, 0};

  // Two identical devices should not be less than each other
  EXPECT_FALSE(cpuMem0_a < cpuMem0_b);
  EXPECT_FALSE(cpuMem0_b < cpuMem0_a);
}

// NicPathType ordering must match GetClosestNicsToGpu filter (pathType > max_path_type)
TEST_F(DeviceTypeTestFixture, NicPathTypeMergeOrdering) {
  EXPECT_LT(NIC_PATH_PIX, NIC_PATH_PXB);
  EXPECT_LT(NIC_PATH_PXB, NIC_PATH_PHB);
  EXPECT_LT(NIC_PATH_PHB, NIC_PATH_SYS);
}

// Test GetNumDevices function
TEST_F(TopologyTestFixture, GetNumDevicesCpu) {
  int numCpus = GetNumDevices(EXE_CPU);
  // Should have at least 1 NUMA node
  EXPECT_GT(numCpus, 0);
}

TEST_F(TopologyTestFixture, GetNumDevicesGpu) {
  int numGpus = GetNumDevices(EXE_GPU);
  // Number should be non-negative
  EXPECT_GE(numGpus, 0);
  // Should match HIP device count
  EXPECT_EQ(numGpus, num_gpus_);
}

TEST_F(TopologyTestFixture, GetNumDevicesNic) {
  int numNics = GetNumDevices(EXE_NIC);
  // Number should be non-negative
  EXPECT_GE(numNics, 0);
}

// Test GetClosestCpuNumaToGpu function
TEST_F(TopologyTestFixture, GetClosestCpuNumaToGpuInvalidIndex) {
  // Test with invalid GPU index (negative)
  int result = GetClosestCpuNumaToGpu(-1);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestCpuNumaToGpuTooLarge) {
  int numGpus = GetNumDevices(EXE_GPU);
  // Test with GPU index >= number of GPUs
  int result = GetClosestCpuNumaToGpu(numGpus + 10);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestCpuNumaToGpuValid) {
  int numGpus = GetNumDevices(EXE_GPU);
  if (numGpus > 0) {
    // Test with valid GPU index (0)
    int result = GetClosestCpuNumaToGpu(0);
    // Should return a valid NUMA node index or -1 if detection failed
    int numCpus = GetNumDevices(EXE_CPU);
    if (result >= 0) {
      EXPECT_LT(result, numCpus);
    }
  }
}

// Test GetClosestCpuNumaToNic function
TEST_F(TopologyTestFixture, GetClosestCpuNumaToNicInvalidIndex) {
  // Test with invalid NIC index (negative)
  int result = GetClosestCpuNumaToNic(-1);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestCpuNumaToNicTooLarge) {
  int numNics = GetNumDevices(EXE_NIC);
  // Test with NIC index >= number of NICs
  int result = GetClosestCpuNumaToNic(numNics + 10);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestCpuNumaToNicValid) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics > 0) {
    // Test with valid NIC index (0)
    int result = GetClosestCpuNumaToNic(0);
    // Should return a valid NUMA node index or -1 if not set
    int numCpus = GetNumDevices(EXE_CPU);
    if (result >= 0) {
      EXPECT_LT(result, numCpus);
    }
  }
}

// Test ParseNicMergeLevel function
TEST_F(TopologyTestFixture, ParseNicMergeLevelKnown) {
  EXPECT_EQ(ParseNicMergeLevel("PIX"), NIC_PATH_PIX);
  EXPECT_EQ(ParseNicMergeLevel("PXB"), NIC_PATH_PXB);
  EXPECT_EQ(ParseNicMergeLevel("PHB"), NIC_PATH_PHB);
  EXPECT_EQ(ParseNicMergeLevel("SYS"), NIC_PATH_SYS);
}

TEST_F(TopologyTestFixture, ParseNicMergeLevelUnknown) {
  EXPECT_EQ(ParseNicMergeLevel("INVALID"), NIC_PATH_SYS);
  EXPECT_EQ(ParseNicMergeLevel(""), NIC_PATH_SYS);
}

// ParseNicList (ROCSHMEM_GDA_NET_FORCE_MERGE comma list)
TEST_F(DeviceTypeTestFixture, ParseNicListCommaSeparated) {
  auto v = ParseNicList("a,b,c");
  ASSERT_EQ(v.size(), 3u);
  EXPECT_EQ(v[0], "a");
  EXPECT_EQ(v[1], "b");
  EXPECT_EQ(v[2], "c");
}

TEST_F(DeviceTypeTestFixture, ParseNicListTrimsSpaces) {
  auto v = ParseNicList(" rocep0 , rocep1 ");
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], "rocep0");
  EXPECT_EQ(v[1], "rocep1");
}

TEST_F(DeviceTypeTestFixture, ParseNicListSingleToken) {
  auto v = ParseNicList("mlx5_0");
  ASSERT_EQ(v.size(), 1u);
  EXPECT_EQ(v[0], "mlx5_0");
}

TEST_F(DeviceTypeTestFixture, ParseNicListEmptyAndSkipsBlankTokens) {
  EXPECT_TRUE(ParseNicList("").empty());
  auto v = ParseNicList("x,, y");
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], "x");
  EXPECT_EQ(v[1], "y");
}

// SelectRankGroup (semicolon-separated rank groups in NET_FORCE_MERGE)
TEST_F(DeviceTypeTestFixture, SelectRankGroupNoSemicolonReturnsWholeSpec) {
  EXPECT_EQ(SelectRankGroup("rocep0,rocep1", 0), "rocep0,rocep1");
  EXPECT_EQ(SelectRankGroup("rocep0,rocep1", 99), "rocep0,rocep1");
}

TEST_F(DeviceTypeTestFixture, SelectRankGroupRoundRobin) {
  std::string spec = "g0a,g0b; g1a,g1b ; g2";
  EXPECT_EQ(SelectRankGroup(spec, 0), "g0a,g0b");
  EXPECT_EQ(SelectRankGroup(spec, 1), "g1a,g1b");
  EXPECT_EQ(SelectRankGroup(spec, 2), "g2");
  EXPECT_EQ(SelectRankGroup(spec, 3), "g0a,g0b");
  EXPECT_EQ(SelectRankGroup(spec, 4), "g1a,g1b");
  EXPECT_EQ(SelectRankGroup(spec, 5), "g2");
}

TEST_F(DeviceTypeTestFixture, SelectRankGroupOnlyBlankGroupsFallsBackToSpec) {
  std::string spec = "  ;  ;  ";
  EXPECT_EQ(SelectRankGroup(spec, 0), spec);
}

// BuildFilteredNicAddresses
TEST_F(TopologyTestFixture, BuildFilteredNicAddressesSizeMatchesDeviceList) {
  int numNics = GetNumDevices(EXE_NIC);
  auto addrs = BuildFilteredNicAddresses(nullptr);
  EXPECT_EQ(addrs.size(), static_cast<size_t>(numNics));
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesIncludeNoMatchAllEmpty) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  auto addrs = BuildFilteredNicAddresses("rocshmem_no_such_nic_zzz");
  ASSERT_EQ(addrs.size(), static_cast<size_t>(numNics));
  for (auto const& a : addrs) {
    EXPECT_TRUE(a.empty());
  }
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesNullHcaListNonEmptyForActive) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  auto addrs = BuildFilteredNicAddresses(nullptr);
  int non_empty = 0;
  for (auto const& a : addrs) {
    if (!a.empty()) non_empty++;
  }
  EXPECT_GT(non_empty, 0) << "expect at least one active IB port with bus id";
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesExcludeNoMatchSameAsNull) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  auto base = BuildFilteredNicAddresses(nullptr);
  auto ex = BuildFilteredNicAddresses("^rocshmem_exclude_no_such_device_zzz");
  ASSERT_EQ(base.size(), ex.size());
  EXPECT_EQ(base, ex);
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesExcludeOneRealName) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  std::vector<std::string> nic_names;
  if (!CollectActiveNicNamesForGpu0(&nic_names)) return;

  auto base = BuildFilteredNicAddresses(nullptr);
  std::string const exclude_list = "^" + nic_names[0];
  auto ex = BuildFilteredNicAddresses(exclude_list.c_str());
  ASSERT_EQ(base.size(), ex.size());
  int nb = 0, ne = 0;
  int excluded_active = 0;
  for (size_t i = 0; i < base.size(); i++) {
    std::string const dev_name = IbvDeviceNameAtIndex(static_cast<int>(i));
    bool const excluded_by_name = (dev_name == nic_names[0]);
    if (!base[i].empty() && excluded_by_name) {
      EXPECT_TRUE(ex[i].empty()) << "excluded NIC " << dev_name << " must lose bus id";
      excluded_active++;
    }
    if (!ex[i].empty()) {
      EXPECT_EQ(ex[i], base[i]) << "non-excluded slot keeps same PCIe bus id";
      ne++;
    }
    if (!base[i].empty()) nb++;
  }
  EXPECT_LE(ne, nb);
  EXPECT_GT(excluded_active, 0) << "exclude list should match at least one active NIC";
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesExcludeTwoRealNames) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  std::vector<std::string> nic_names;
  if (!CollectActiveNicNamesForGpu0(&nic_names)) return;
  std::string n0, n1;
  if (!TwoDistinctNicNames(nic_names, &n0, &n1)) GTEST_SKIP() << "need two distinct ibv device names";

  auto base = BuildFilteredNicAddresses(nullptr);
  std::string const exclude_list = "^" + n0 + "," + n1;
  auto ex = BuildFilteredNicAddresses(exclude_list.c_str());
  ASSERT_EQ(base.size(), ex.size());
  int excluded_active = 0;
  for (size_t i = 0; i < base.size(); i++) {
    std::string const dev_name = IbvDeviceNameAtIndex(static_cast<int>(i));
    bool const excluded_by_name = (dev_name == n0 || dev_name == n1);
    if (!base[i].empty() && excluded_by_name) {
      EXPECT_TRUE(ex[i].empty()) << "excluded NIC " << dev_name << " must lose bus id";
      excluded_active++;
    }
    if (!ex[i].empty())
      EXPECT_EQ(ex[i], base[i]);
  }
  EXPECT_GE(excluded_active, 2) << "both excluded names should match active NIC entries";
}

TEST_F(TopologyTestFixture, BuildFilteredNicAddressesIncludeTwoRealNames) {
  int numNics = GetNumDevices(EXE_NIC);
  if (numNics == 0) return;
  std::vector<std::string> nic_names;
  if (!CollectActiveNicNamesForGpu0(&nic_names)) return;
  std::string n0, n1;
  if (!TwoDistinctNicNames(nic_names, &n0, &n1)) GTEST_SKIP() << "need two distinct ibv device names";

  auto base = BuildFilteredNicAddresses(nullptr);
  std::string const include_list = n0 + "," + n1;
  auto inc = BuildFilteredNicAddresses(include_list.c_str());
  ASSERT_EQ(base.size(), inc.size());
  int nb = 0, ni = 0;
  for (size_t i = 0; i < base.size(); i++) {
    std::string const dev_name = IbvDeviceNameAtIndex(static_cast<int>(i));
    bool const in_list = (dev_name == n0 || dev_name == n1);
    if (!base[i].empty()) {
      if (in_list) {
        EXPECT_EQ(inc[i], base[i]) << "included NIC " << dev_name << " keeps bus id";
        ni++;
      } else {
        EXPECT_TRUE(inc[i].empty()) << "NIC " << dev_name << " not in include list must be cleared";
      }
    } else {
      EXPECT_TRUE(inc[i].empty());
    }
    if (!base[i].empty()) nb++;
  }
  EXPECT_LE(ni, nb);
  EXPECT_GE(ni, 2) << "include list with two real names should keep at least two active entries";
}

// Test GetClosestNicToGpu function (single-NIC selection)
TEST_F(TopologyTestFixture, GetClosestNicToGpuInvalidIndex) {
  int result = GetClosestNicToGpu(-1, nullptr, nullptr);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestNicToGpuTooLarge) {
  int numGpus = GetNumDevices(EXE_GPU);
  int result = GetClosestNicToGpu(numGpus + 10, nullptr, nullptr);
  EXPECT_EQ(result, -1);
}

TEST_F(TopologyTestFixture, GetClosestNicToGpuValid) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    std::string name;
    int result = GetClosestNicToGpu(0, nullptr, &name);
    if (result >= 0) {
      EXPECT_LT(result, numNics);
      EXPECT_FALSE(name.empty());
    }
  }
}

TEST_F(TopologyTestFixture, GetClosestNicToGpuConsistency) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    int r1 = GetClosestNicToGpu(0, nullptr, nullptr);
    int r2 = GetClosestNicToGpu(0, nullptr, nullptr);
    EXPECT_EQ(r1, r2);
  }
}

// Test GetClosestNicsToGpu function (multi-NIC selection)
TEST_F(TopologyTestFixture, GetClosestNicsToGpuInvalidIndex) {
  std::vector<std::string> names;
  int result = GetClosestNicsToGpu(-1, nullptr, NIC_PATH_PIX, names);
  EXPECT_EQ(result, -1);
  EXPECT_TRUE(names.empty());
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuTooLarge) {
  int numGpus = GetNumDevices(EXE_GPU);
  std::vector<std::string> names;
  int result = GetClosestNicsToGpu(numGpus + 10, nullptr, NIC_PATH_PIX, names);
  EXPECT_EQ(result, -1);
  EXPECT_TRUE(names.empty());
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuValid) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    std::vector<std::string> names;
    int result = GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, names);
    EXPECT_GT(result, 0);
    EXPECT_EQ(result, static_cast<int>(names.size()));
    EXPECT_FALSE(names[0].empty());
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuWithExcludeList) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    const char* excludeList = "^mlx5_0";
    std::vector<std::string> names;
    int result = GetClosestNicsToGpu(0, excludeList, NIC_PATH_SYS, names);
    if (result == -1) {
      EXPECT_TRUE(names.empty());
    } else {
      EXPECT_GT(result, 0);
      EXPECT_EQ(result, static_cast<int>(names.size()));
      for (auto const& n : names) {
        EXPECT_NE(n, "mlx5_0");
      }
    }
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuConsistency) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    std::vector<std::string> names1, names2;
    GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, names1);
    GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, names2);
    EXPECT_EQ(names1, names2);
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuClosestFirst) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 1) {
    std::vector<std::string> names_all, names_pix;
    int count_all = GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, names_all);
    int count_pix = GetClosestNicsToGpu(0, nullptr, NIC_PATH_PIX, names_pix);
    if (count_all > 0 && count_pix > 0) {
      EXPECT_EQ(names_all[0], names_pix[0])
          << "Closest NIC should be the same regardless of max_path_type";
    }
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuMergeLevelFilters) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    std::vector<std::string> names_sys, names_pix;
    int count_sys = GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, names_sys);
    int count_pix = GetClosestNicsToGpu(0, nullptr, NIC_PATH_PIX, names_pix);
    if (count_sys > 0 && count_pix > 0) {
      EXPECT_GE(count_sys, count_pix)
          << "SYS level should return >= NICs than PIX level";
    }
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuPhbPxbSubsetOfSys) {
  int numGpus = GetNumDevices(EXE_GPU);
  int numNics = GetNumDevices(EXE_NIC);

  if (numGpus > 0 && numNics > 0) {
    std::vector<std::string> sys_nics, phb_nics, pxb_nics;
    int n_sys = GetClosestNicsToGpu(0, nullptr, NIC_PATH_SYS, sys_nics);
    int n_phb = GetClosestNicsToGpu(0, nullptr, NIC_PATH_PHB, phb_nics);
    int n_pxb = GetClosestNicsToGpu(0, nullptr, NIC_PATH_PXB, pxb_nics);
    if (n_sys > 0 && n_phb > 0) {
      EXPECT_GE(n_sys, n_phb);
    }
    if (n_phb > 0 && n_pxb > 0) {
      EXPECT_GE(n_phb, n_pxb);
    }
  }
}

TEST_F(TopologyTestFixture, GetClosestNicsToGpuIncludeListNoMatch) {
  int numGpus = GetNumDevices(EXE_GPU);

  if (numGpus > 0) {
    std::vector<std::string> names;
    int result = GetClosestNicsToGpu(
        0, "rocshmem_nonexistent_nic", NIC_PATH_SYS, names);
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(names.empty());
  }
}

TEST_F(TopologyTestFixture, ComputeGpuNicPathTypeSyntheticBusId) {
  int numGpus = GetNumDevices(EXE_GPU);

  if (numGpus > 0) {
    const std::string fakeNicBus("0000:fe:00.0");
    NicPathType a = ComputeGpuNicPathType(0, fakeNicBus, -1);
    NicPathType b = ComputeGpuNicPathType(0, fakeNicBus, -1);
    EXPECT_EQ(a, b);
    int v = static_cast<int>(a);
    EXPECT_GE(v, static_cast<int>(NIC_PATH_PIX));
    EXPECT_LE(v, static_cast<int>(NIC_PATH_SYS));
  }
}

// Test DisplayTopology function (just verify it doesn't crash)
TEST_F(TopologyTestFixture, DisplayTopologyNoCrash) {
  // This test just verifies the function can be called without crashing
  // Output is redirected to stdout, so we can't easily test it
  testing::internal::CaptureStdout();
  DisplayTopology(false);
  std::string output = testing::internal::GetCapturedStdout();
  // Should contain some expected text
  EXPECT_TRUE(output.find("Detected Topology") != std::string::npos ||
              output.find("NumCpus") != std::string::npos);
}

TEST_F(TopologyTestFixture, DisplayTopologyCSVFormat) {
  testing::internal::CaptureStdout();
  DisplayTopology(true);
  std::string output = testing::internal::GetCapturedStdout();
  // CSV format should contain NumCpus, NumGpus, NumNics
  EXPECT_TRUE(output.find("NumCpus,") != std::string::npos);
  EXPECT_TRUE(output.find("NumGpus,") != std::string::npos);
  EXPECT_TRUE(output.find("NumNics,") != std::string::npos);
}

// Test GidPriority enum ordering
TEST_F(DeviceTypeTestFixture, GidPriorityOrdering) {
  // Verify the priority ordering
  EXPECT_LT(static_cast<int>(GidPriority::UNKNOWN),
            static_cast<int>(GidPriority::ROCEV1_LINK_LOCAL));
  EXPECT_LT(static_cast<int>(GidPriority::ROCEV1_LINK_LOCAL),
            static_cast<int>(GidPriority::ROCEV2_LINK_LOCAL));
  EXPECT_LT(static_cast<int>(GidPriority::ROCEV2_LINK_LOCAL),
            static_cast<int>(GidPriority::ROCEV1_IPV6));
  EXPECT_LT(static_cast<int>(GidPriority::ROCEV1_IPV6),
            static_cast<int>(GidPriority::ROCEV2_IPV6));
  EXPECT_LT(static_cast<int>(GidPriority::ROCEV2_IPV6),
            static_cast<int>(GidPriority::ROCEV1_IPV4));
  EXPECT_LT(static_cast<int>(GidPriority::ROCEV1_IPV4),
            static_cast<int>(GidPriority::ROCEV2_IPV4));
}

// Test multiple GPUs if available
TEST_F(TopologyTestFixture, MultipleGpuTopology) {
  int numGpus = GetNumDevices(EXE_GPU);

  if (numGpus > 1) {
    // Test that each GPU can find a closest NUMA node
    for (int i = 0; i < numGpus; i++) {
      int numaNode = GetClosestCpuNumaToGpu(i);
      // Should be valid or -1
      int numCpus = GetNumDevices(EXE_CPU);
      if (numaNode >= 0) {
        EXPECT_LT(numaNode, numCpus);
      }
    }
  }
}

//==============================================================================
// PCIe Tree Tests with Theoretical Topology
//==============================================================================

// Test ExtractBusNumber function
TEST_F(PCIeTreeTestFixture, ExtractBusNumberValid) {
  // Test valid PCIe addresses
  EXPECT_EQ(ExtractBusNumber("0000:02:00.0"), 0x02);
  EXPECT_EQ(ExtractBusNumber("0000:05:00.0"), 0x05);
  EXPECT_EQ(ExtractBusNumber("0000:42:00.0"), 0x42);
  EXPECT_EQ(ExtractBusNumber("0000:45:00.0"), 0x45);
  EXPECT_EQ(ExtractBusNumber("0000:ff:1f.7"), 0xff);
}

TEST_F(PCIeTreeTestFixture, ExtractBusNumberInvalid) {
  // Test invalid PCIe addresses
  EXPECT_EQ(ExtractBusNumber("invalid"), -1);
  EXPECT_EQ(ExtractBusNumber(""), -1);
  EXPECT_EQ(ExtractBusNumber("0000:GG:00.0"), -1);
}

// Test GetBusIdDistance function
TEST_F(PCIeTreeTestFixture, GetBusIdDistanceSameSocket) {
  // GPU 0 and NIC 0 are on the same switch (bus 0x02 and 0x03)
  int distance = GetBusIdDistance(
    gpu_addresses_[0], nic_addresses_[0]);
  EXPECT_EQ(distance, 1);  // |0x02 - 0x03| = 1
}

TEST_F(PCIeTreeTestFixture, GetBusIdDistanceDifferentSocket) {
  // GPU 0 (bus 0x02) and GPU 2 (bus 0x42)
  int distance = GetBusIdDistance(
    gpu_addresses_[0], gpu_addresses_[2]);
  EXPECT_EQ(distance, 0x40);  // |0x02 - 0x42| = 0x40
}

TEST_F(PCIeTreeTestFixture, GetBusIdDistanceZero) {
  // Same address
  int distance = GetBusIdDistance(
    gpu_addresses_[0], gpu_addresses_[0]);
  EXPECT_EQ(distance, 0);
}

TEST_F(PCIeTreeTestFixture, GetBusIdDistanceInvalid) {
  // Invalid address
  int distance = GetBusIdDistance(
    "invalid", gpu_addresses_[0]);
  EXPECT_EQ(distance, -1);
}

// Test GetLcaBetweenNodes function
TEST_F(PCIeTreeTestFixture, GetLcaSameSwitch) {
  // GPU 0 and NIC 0 are on the same PCIe switch
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], nic_addresses_[0]);
  ASSERT_NE(lca, nullptr);
  // LCA should be the PCIe switch (0000:01:00.0)
  EXPECT_EQ(lca->address, "0000:01:00.0");
}

TEST_F(PCIeTreeTestFixture, GetLcaSameSocket) {
  // GPU 0 (switch 0) and GPU 1 (switch 1) are on the same socket
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], gpu_addresses_[1]);
  ASSERT_NE(lca, nullptr);
  // LCA should be the CPU socket (0000:00:01.0)
  EXPECT_EQ(lca->address, "0000:00:01.0");
}

TEST_F(PCIeTreeTestFixture, GetLcaDifferentSocket) {
  // GPU 0 (socket 0) and GPU 2 (socket 1) are on different sockets
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], gpu_addresses_[2]);
  ASSERT_NE(lca, nullptr);
  // LCA should be the root
  EXPECT_EQ(lca->address, "root");
}

TEST_F(PCIeTreeTestFixture, GetLcaSameNode) {
  // Same node - should return the node itself
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], gpu_addresses_[0]);
  ASSERT_NE(lca, nullptr);
  EXPECT_EQ(lca->address, gpu_addresses_[0]);
}

TEST_F(PCIeTreeTestFixture, GetLcaNotFound) {
  // Non-existent node
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, "0000:99:00.0", gpu_addresses_[0]);
  EXPECT_EQ(lca, nullptr);
}

// Test GetLcaDepth function
TEST_F(PCIeTreeTestFixture, GetLcaDepthRoot) {
  int depth = GetLcaDepth("root", &root_);
  EXPECT_EQ(depth, 0);
}

TEST_F(PCIeTreeTestFixture, GetLcaDepthSocket) {
  int depth = GetLcaDepth("0000:00:01.0", &root_);
  EXPECT_EQ(depth, 1);
}

TEST_F(PCIeTreeTestFixture, GetLcaDepthSwitch) {
  int depth = GetLcaDepth("0000:01:00.0", &root_);
  EXPECT_EQ(depth, 2);
}

TEST_F(PCIeTreeTestFixture, GetLcaDepthDevice) {
  int depth = GetLcaDepth(gpu_addresses_[0], &root_);
  EXPECT_EQ(depth, 3);
}

TEST_F(PCIeTreeTestFixture, GetLcaDepthNotFound) {
  int depth = GetLcaDepth("0000:99:00.0", &root_);
  EXPECT_EQ(depth, -1);
}

// Test GetNearestDevicesInTree function
TEST_F(PCIeTreeTestFixture, GetNearestDevicesSameSwitch) {
  // GPU 0 should be closest to NIC 0 (same switch)
  std::set<int> nearest = GetNearestDevicesInTree(
    gpu_addresses_[0], nic_addresses_, &root_);

  ASSERT_EQ(nearest.size(), 1);
  EXPECT_EQ(*nearest.begin(), 0);  // NIC 0
}

TEST_F(PCIeTreeTestFixture, GetNearestDevicesSameSocket) {
  // GPU 0 should prefer NICs on the same socket
  // When NIC 0 is excluded, should prefer NIC 1 (same socket) over NIC 2/3 (different socket)
  std::vector<std::string> nicsExcludingFirst = {
    "",                // NIC 0 excluded
    nic_addresses_[1], // NIC 1 (same socket)
    nic_addresses_[2], // NIC 2 (different socket)
    nic_addresses_[3]  // NIC 3 (different socket)
  };

  std::set<int> nearest = GetNearestDevicesInTree(
    gpu_addresses_[0], nicsExcludingFirst, &root_);

  ASSERT_EQ(nearest.size(), 1);
  EXPECT_EQ(*nearest.begin(), 1);  // NIC 1
}

TEST_F(PCIeTreeTestFixture, GetNearestDevicesCrossSocket) {
  // GPU 2 (socket 1) should be closest to NIC 2 or NIC 3 (both on socket 1)
  std::set<int> nearest = GetNearestDevicesInTree(
    gpu_addresses_[2], nic_addresses_, &root_);

  ASSERT_EQ(nearest.size(), 1);
  EXPECT_EQ(*nearest.begin(), 2);  // NIC 2 (same switch as GPU 2)
}

TEST_F(PCIeTreeTestFixture, GetNearestDevicesMultipleMatches) {
  // Create scenario with equidistant devices
  // GPU 0 with NICs from different sockets only
  std::vector<std::string> crossSocketNics = {
    "",                // Exclude socket 0 NICs
    "",
    nic_addresses_[2], // NIC 2 (socket 1, switch 2)
    nic_addresses_[3]  // NIC 3 (socket 1, switch 3)
  };

  std::set<int> nearest = GetNearestDevicesInTree(
    gpu_addresses_[0], crossSocketNics, &root_);

  // Should match both NIC 2 and NIC 3 with equal priority
  EXPECT_GE(nearest.size(), 1);
  // Both are equally far (different socket)
}

TEST_F(PCIeTreeTestFixture, GetNearestDevicesEmptyList) {
  std::vector<std::string> emptyList = {"", "", "", ""};

  std::set<int> nearest = GetNearestDevicesInTree(
    gpu_addresses_[0], emptyList, &root_);

  EXPECT_EQ(nearest.size(), 0);
}

// Test PCIe tree structure verification
TEST_F(PCIeTreeTestFixture, TreeStructureHasRoot) {
  EXPECT_EQ(root_.address, "root");
  EXPECT_EQ(root_.description, "PCIe Root Complex");
  EXPECT_GT(root_.children.size(), 0);
}

TEST_F(PCIeTreeTestFixture, TreeStructureHasTwoSockets) {
  // Should have 2 CPU sockets
  EXPECT_EQ(root_.children.size(), 2);
}

TEST_F(PCIeTreeTestFixture, TreeStructureSocket0) {
  // Find socket 0
  bool found = false;
  for (auto const& socket : root_.children) {
    if (socket.address == "0000:00:01.0") {
      found = true;
      // Socket 0 should have 2 switches
      EXPECT_EQ(socket.children.size(), 2);
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(PCIeTreeTestFixture, TreeStructureSocket1) {
  // Find socket 1
  bool found = false;
  for (auto const& socket : root_.children) {
    if (socket.address == "0000:40:00.0") {
      found = true;
      // Socket 1 should have 2 switches
      EXPECT_EQ(socket.children.size(), 2);
    }
  }
  EXPECT_TRUE(found);
}

// Test all GPU-NIC affinity mappings in theoretical topology
TEST_F(PCIeTreeTestFixture, GpuNicAffinityMapping) {
  // Expected optimal NIC for each GPU
  // GPU 0 -> NIC 0 (same switch)
  // GPU 1 -> NIC 1 (same switch)
  // GPU 2 -> NIC 2 (same switch)
  // GPU 3 -> NIC 3 (same switch)

  for (size_t gpu_idx = 0; gpu_idx < gpu_addresses_.size(); gpu_idx++) {
    std::set<int> nearest = GetNearestDevicesInTree(
      gpu_addresses_[gpu_idx], nic_addresses_, &root_);

    ASSERT_EQ(nearest.size(), 1) << "GPU " << gpu_idx;
    EXPECT_EQ(*nearest.begin(), gpu_idx) << "GPU " << gpu_idx;
  }
}

// Test bus distance calculations for all pairs
TEST_F(PCIeTreeTestFixture, BusDistanceMatrix) {
  // Test distances between all GPUs
  std::vector<std::vector<int>> expected_distances = {
    {0,  3,  0x40, 0x43},  // GPU 0 to GPU 0,1,2,3
    {3,  0,  0x3D, 0x40},  // GPU 1 to GPU 0,1,2,3
    {0x40, 0x3D, 0, 3},    // GPU 2 to GPU 0,1,2,3
    {0x43, 0x40, 3, 0}     // GPU 3 to GPU 0,1,2,3
  };

  for (size_t i = 0; i < gpu_addresses_.size(); i++) {
    for (size_t j = 0; j < gpu_addresses_.size(); j++) {
      int distance = GetBusIdDistance(
        gpu_addresses_[i], gpu_addresses_[j]);
      EXPECT_EQ(distance, expected_distances[i][j])
        << "Distance between GPU " << i << " and GPU " << j;
    }
  }
}

// Test LCA depth verification for known pairs
TEST_F(PCIeTreeTestFixture, LcaDepthVerification) {
  // Devices on same switch: LCA depth should be 2 (switch level)
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], nic_addresses_[0]);
  ASSERT_NE(lca, nullptr);
  int depth = GetLcaDepth(lca->address, &root_);
  EXPECT_EQ(depth, 2);  // Switch level

  // Devices on same socket: LCA depth should be 1 (socket level)
  lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], gpu_addresses_[1]);
  ASSERT_NE(lca, nullptr);
  depth = GetLcaDepth(lca->address, &root_);
  EXPECT_EQ(depth, 1);  // Socket level

  // Devices on different sockets: LCA depth should be 0 (root level)
  lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], gpu_addresses_[2]);
  ASSERT_NE(lca, nullptr);
  depth = GetLcaDepth(lca->address, &root_);
  EXPECT_EQ(depth, 0);  // Root level
}

TEST_F(PCIeTreeTestFixture, PathTypeFromLcaDepth) {
  int gpuDepth = GetLcaDepth(gpu_addresses_[0], &root_);
  ASSERT_EQ(gpuDepth, 3);

  // Same switch (PIX): LCA depth 2, hops = 3 - 2 = 1
  PCIeNode const* lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], nic_addresses_[0]);
  ASSERT_NE(lca, nullptr);
  int hops = gpuDepth - GetLcaDepth(lca->address, &root_);
  EXPECT_EQ(hops, 1);

  // Different switch, same socket (PXB): LCA depth 1, hops = 3 - 1 = 2
  lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], nic_addresses_[1]);
  ASSERT_NE(lca, nullptr);
  hops = gpuDepth - GetLcaDepth(lca->address, &root_);
  EXPECT_EQ(hops, 2);

  // Different sockets: LCA depth 0 (root), falls through to NUMA check
  lca = GetLcaBetweenNodes(
    &root_, gpu_addresses_[0], nic_addresses_[2]);
  ASSERT_NE(lca, nullptr);
  EXPECT_EQ(GetLcaDepth(lca->address, &root_), 0);
}
