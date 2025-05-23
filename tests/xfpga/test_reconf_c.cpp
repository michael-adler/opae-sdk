// Copyright(c) 2017-2022, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#define NO_OPAE_C
#include "mock/opae_fixtures.h"
KEEP_XFPGA_SYMBOLS
#include "mock/test_utils.h"

extern "C" {
#include "bitstream_int.h"
#include "fpga-dfl.h"
#include "reconf_int.h"
#include "xfpga.h"
#include "sysfs_int.h"

fpga_result open_accel(fpga_handle handle, fpga_token *token, fpga_handle *accel);
fpga_result clear_port_errors(fpga_handle handle);
fpga_result validate_bitstream(fpga_handle, const uint8_t *bitstream, 
                               size_t bitstream_len, int *header_len);
int xfpga_plugin_initialize(void);
int xfpga_plugin_finalize(void);
}

using namespace opae::testing;

class reconf_c : public opae_device_p<xfpga_> {
 protected:

  virtual void OPAEInitialize() override {
    ASSERT_EQ(xfpga_plugin_initialize(), 0);
  }

  virtual void OPAEFinalize() override {
    ASSERT_EQ(xfpga_plugin_finalize(), 0);
  }

  virtual void SetUp() override {
    opae_device_p<xfpga_>::SetUp();

    test_device device = platform_.devices[0];

    bitstream_valid_ = system_->assemble_gbs_header(device);
    std::string version = "630";

    auto fme_guid = device.fme_guid;
    auto afu_guid = device.afu_guid;

    // clang-format off
    auto bitstream_j = jobject
    ("version", version)
    ("afu-image", jobject
                  ("interface-uuid", fme_guid)
                  ("magic-no", int32_t(488605312))
                  ("accelerator-clusters", {
                                             jobject
                                             ("total-contexts", int32_t(1))
                                             ("name", "nlb")
                                             ("accelerator-type-uuid", afu_guid)
                                            }
                  )
    )
    ("platform-name", "");
    // clang-format on

    bitstream_valid_no_clk_ = system_->assemble_gbs_header(device, bitstream_j.c_str());
    bitstream_j.put();
  }

  std::vector<uint8_t> bitstream_valid_;
  std::vector<uint8_t> bitstream_valid_no_clk_;
};

/**
* @test    set_afu_userclock
* @brief   Tests: set_afu_userclock
* @details set_afu_userclock sets afu user clock
*          Returns FPGA_OK if parameters are valid. Returns
*          error code if invalid user clock or handle.
*/
TEST_P(reconf_c, set_afu_userclock) {
  fpga_result result;

  // Null handle
  result = set_afu_userclock(NULL, 0, 0);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Invalid params
  result = set_afu_userclock(device_, 0, 0);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);
}

/**
* @test    fpga_reconf_slot
* @brief   Tests: fpgaReconfigureSlot
* @details Returns FPGA_OK if bitstream is valid and is able
*          to reconfigure fpga. Returns error code if
*          bitstream, handle, or parameters are invalid.
*/
TEST_P(reconf_c, fpga_reconf_slot) {
  fpga_result result;
  uint8_t bitstream_empty[] = "";
  uint8_t bitstream_invalid_guid[] =
      "Xeon\xb7GBSv001\53\02\00\00{\"version\": 640, \"afu-image\": \
      {\"clock-frequency-high\": 312, \"clock-frequency-low\": 156, \
      \"power\": 50, \"interface-uuid\": \"1a422218-6dba-448e-b302-425cbcde1406\", \
      \"magic-no\": 488605312, \"accelerator-clusters\": [{\"total-contexts\": 1,\
      \"name\": \"nlb_400\", \"accelerator-type-uuid\":\
      \"d8424dc4-a4a3-c413-f89e-433683f9040b\"}]}, \"platform-name\": \"MCP\"}";
  uint8_t bitstream_invalid_json[] =
      "XeonFPGA\xb7GBSv001\53\02{\"version\": \"afu-image\"}";
  size_t bitstream_valid_len =
      get_bitstream_header_len(bitstream_valid_.data());
  uint32_t slot = 0;
  int flags = 0;

  // Invalid bitstream - null
  result = xfpga_fpgaReconfigureSlot(device_, slot, NULL, 0, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Invalid bitstream - empty
  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_empty, 0, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Invalid bitstream - invalid guid
  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_invalid_guid,
                                     bitstream_valid_len, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Invalid bitstream - invalid json
  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_invalid_json,
                                     bitstream_valid_len, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Null handle
  result = xfpga_fpgaReconfigureSlot(NULL, slot, bitstream_valid_.data(),
                                     bitstream_valid_.size(), flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Invalid handle file descriptor
  auto &no_clk_arr = bitstream_valid_no_clk_;
  struct _fpga_handle *handle = (struct _fpga_handle *)device_;
  uint32_t fddev = handle->fddev;

  handle->fddev = -1;

  result =
      xfpga_fpgaReconfigureSlot(device_, slot, no_clk_arr.data(), no_clk_arr.size(), flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  handle->fddev = fddev;
}

/**
* @test    open_accel
* @brief   Tests: open_accel_01
* @details Returns FPGA_INVALID_PARAM when calling open_accel with
*          an invalid handle.
*/
TEST_P(reconf_c, open_accel_01) {
  fpga_result result;
  fpga_token tok = nullptr;
  fpga_handle accel = nullptr;

  // Null handle
  result = open_accel(NULL, &tok, &accel);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  // Valid handle
  result = open_accel(device_, &tok, &accel);
  EXPECT_EQ(result, FPGA_OK);

  EXPECT_EQ(xfpga_fpgaClose(accel), FPGA_OK);
  EXPECT_EQ(xfpga_fpgaDestroyToken(&tok), FPGA_OK);

  // Invalid object type
  struct _fpga_handle *handle = (struct _fpga_handle *)device_;
  struct _fpga_token *token = (struct _fpga_token *)&handle->token;

  handle->token = NULL;

  result = open_accel(device_, &tok, &accel);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  handle->token = token;
}

/**
* @test    open_accel
* @brief   Tests: open_accel_02
* @details Returns FPGA_BUSY when calling open_accel with
*          an opened accel handle.
*/
TEST_P(reconf_c, open_accel_02) {
  fpga_properties filter_accel = nullptr;
  std::array<fpga_token, 2> tokens_accel = {{nullptr,nullptr}};
  fpga_handle handle_accel = nullptr;
  fpga_handle accel = nullptr;
  uint32_t num_matches_accel;

  ASSERT_EQ(xfpga_fpgaGetProperties(nullptr, &filter_accel), FPGA_OK);
  ASSERT_EQ(fpgaPropertiesSetObjectType(filter_accel, FPGA_ACCELERATOR),
            FPGA_OK);
  ASSERT_EQ(xfpga_fpgaEnumerate(&filter_accel, 1, tokens_accel.data(),
                                tokens_accel.size(), &num_matches_accel),
            FPGA_OK);
  ASSERT_EQ(FPGA_OK, xfpga_fpgaOpen(tokens_accel[0], &handle_accel, 0));

  EXPECT_NE(device_, nullptr);
  EXPECT_NE(handle_accel, nullptr);
  fpga_token tok = nullptr;
  auto result = open_accel(handle_accel, &tok, &accel);
  EXPECT_EQ(result, FPGA_BUSY);

  EXPECT_EQ(accel, nullptr);
  if (tok) {
    EXPECT_EQ(xfpga_fpgaDestroyToken(&tok), FPGA_OK);
  }
  EXPECT_EQ(fpgaDestroyProperties(&filter_accel), FPGA_OK);
  EXPECT_EQ(xfpga_fpgaClose(handle_accel), FPGA_OK);
  for (auto &t : tokens_accel) {
    if (t != nullptr) {
      EXPECT_EQ(xfpga_fpgaDestroyToken(&t), FPGA_OK);
      t = nullptr;
    }
  }
}

/**
 * @test validate_bitstream
 * @brief Tests: validate_bitstream
 * @details: When validate_bitstream is given an invalid
 *           bitstream header length, the function returns
 *           FPGA_EXCEPTION.
 */
TEST_P(reconf_c, validate_bitstream) {
  uint8_t bitstream_invalid_len[] = "XeonFPGA\xb7GBSv001\255\255\255\255";
  size_t bitstream_len = sizeof(bitstream_invalid_len) / sizeof(uint8_t);
  int header_len;
  fpga_result result;

  result = validate_bitstream(device_, bitstream_invalid_len,
                              bitstream_len, &header_len);
  EXPECT_EQ(FPGA_EXCEPTION, result);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(reconf_c);
INSTANTIATE_TEST_SUITE_P(reconf, reconf_c,
                         ::testing::ValuesIn(test_platform::platforms({})));

class reconf_c_mock_p : public opae_device_p<xfpga_> {
 protected:

  virtual void OPAEInitialize() override {
    ASSERT_EQ(xfpga_plugin_initialize(), 0);
  }

  virtual void OPAEFinalize() override {
    ASSERT_EQ(xfpga_plugin_finalize(), 0);
  }

  virtual void SetUp() override {
    opae_device_p<xfpga_>::SetUp();

    test_device device = platform_.devices[0];

    // assemble valid bitstream header
    auto fme_guid = device.fme_guid;
    auto afu_guid = device.afu_guid;

    auto bitstream_j = jobject
    ("version", "640")
    ("afu-image", jobject
                  ("interface-uuid", fme_guid)
                  ("magic-no", int32_t(488605312))
                  ("accelerator-clusters", {
                                             jobject
                                             ("total-contexts", int32_t(1))
                                             ("name", "nlb")
                                             ("accelerator-type-uuid", afu_guid)
                                            }
                  )
    )
    ("platform-name", "");

    bitstream_valid_ = system_->assemble_gbs_header(device, bitstream_j.c_str());
    bitstream_j.put();
  }

  std::vector<uint8_t> bitstream_valid_;
};

/**
 * @test    set_afu_userclock
 * @brief   Tests: set_afu_userclock
 * @details When given valid parameters, set_afu_userclock
 *          returns FPGA_NOT_FOUND on mock platforms.
 */
TEST_P(reconf_c_mock_p, set_afu_userclock) {
  EXPECT_EQ(set_afu_userclock(device_, 312, 156), FPGA_NOT_FOUND);
}

/**
 * @test    fpga_reconf_slot
 * @brief   Tests: fpgaReconfigureSlot
 * @details Returns FPGA_OK if bitstream is valid and is able
 *          to reconfigure the fpga.
 */
TEST_P(reconf_c_mock_p, fpga_reconf_slot) {
  fpga_result result;
  uint32_t slot = 0;
  int flags = 0;

  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_valid_.data(),
                                     bitstream_valid_.size(), flags);
  EXPECT_EQ(result, FPGA_OK);
}

/**
 * @test    fpga_reconf_slot_einval
 * @brief   Tests: fpgaReconfigureSlot
 * @details Register an ioctl handler that returns -1 and sets
 *          errno to EINVAL. fpgaReconfigureSlot should return
 *          FPGA_INVALID_PARAM.
 */
TEST_P(reconf_c_mock_p, fpga_reconf_slot_einval) {
  fpga_result result;
  uint32_t slot = 0;
  int flags = 0;

  // register an ioctl handler that will return -1 and set errno to EINVAL
  system_->register_ioctl_handler(DFL_FPGA_FME_PORT_PR, dummy_ioctl<-1, EINVAL>);
  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_valid_.data(),
                                     bitstream_valid_.size(), flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);
}

/**
 * @test    fpga_reconf_slot_enotsup
 * @brief   Tests: fpgaReconfigureSlot
 * @details Register an ioctl handler that returns -1 and sets
 *          errno to ENOTSUP. fpgaReconfigureSlot should return
 *          FPGA_EXCEPTION.
 */
TEST_P(reconf_c_mock_p, fpga_reconf_slot_enotsup) {
  fpga_result result;
  uint32_t slot = 0;
  int flags = 0;

  // register an ioctl handler that will return -1 and set errno to ENOTSUP
  system_->register_ioctl_handler(DFL_FPGA_FME_PORT_PR, dummy_ioctl<-1, ENOTSUP>);
  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_valid_.data(),
                                     bitstream_valid_.size(), flags);
  EXPECT_EQ(result, FPGA_EXCEPTION);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(reconf_c_mock_p);
INSTANTIATE_TEST_SUITE_P(reconf, reconf_c_mock_p,
                         ::testing::ValuesIn(test_platform::mock_platforms({ "dfl-n3000","dfl-d5005" })));

class reconf_c_hw_skx_p : public reconf_c {};

/**
 * @test    set_afu_userclock
 * @brief   Tests: set_afu_userclock
 * @details Given valid parameters set_afu_userlock returns
 *          FPGA_OK on mcp hw platforms.
 */
TEST_P(reconf_c_hw_skx_p, set_afu_userclock) {
  EXPECT_EQ(set_afu_userclock(device_, 312, 156), FPGA_OK);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(reconf_c_hw_skx_p);
INSTANTIATE_TEST_SUITE_P(reconf, reconf_c_hw_skx_p,
                         ::testing::ValuesIn(test_platform::hw_platforms({ "dfl-d5005" })));

class reconf_c_hw_dcp_p : public reconf_c {};

/**
 * @test    set_afu_userclock
 * @brief   Tests: set_afu_userclock
 * @details Given valid parameters set_afu_userlock returns
 *          FPGA_NOT_SUPPORTED on dcp hw platforms.
 */
TEST_P(reconf_c_hw_dcp_p, set_afu_userclock) {
  EXPECT_EQ(set_afu_userclock(device_, 312, 156), FPGA_NOT_SUPPORTED);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(reconf_c_hw_dcp_p);
INSTANTIATE_TEST_SUITE_P(reconf, reconf_c_hw_dcp_p,
                         ::testing::ValuesIn(test_platform::hw_platforms({ "dfl-d5005" })));

/**
* @test    clear_port_errors
* @brief   Tests: clear_port_errors
* @details Returns FPGA_OK if handle is valid and
*          can clear port errors.
*/
TEST(reconf, clear_port_errors) {
  fpga_result result;

  // Null handle
  result = clear_port_errors(NULL);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);
}

class reconf_c_hw_p : public opae_device_p<xfpga_> {
 protected:

  virtual void OPAEInitialize() override {
    ASSERT_EQ(xfpga_plugin_initialize(), 0);
  }

  virtual void OPAEFinalize() override {
    ASSERT_EQ(xfpga_plugin_finalize(), 0);
  }

  virtual void SetUp() override {
    opae_device_p<xfpga_>::SetUp();

    test_device device = platform_.devices[0];

    // assemble valid bitstream header
    auto fme_guid = device.fme_guid;
    auto afu_guid = device.afu_guid;

    auto bitstream_j = jobject
    ("version", "640")
    ("afu-image", jobject
                  ("interface-uuid", fme_guid)
                  ("magic-no", int32_t(488605312))
                  ("accelerator-clusters", {
                                             jobject
                                             ("total-contexts", int32_t(1))
                                             ("name", "nlb")
                                             ("accelerator-type-uuid", afu_guid)
                                            }
                  )
    )
    ("platform-name", "");

    bitstream_valid_ = system_->assemble_gbs_header(device, bitstream_j.c_str());
    bitstream_j.put();
  }

  std::vector<uint8_t> bitstream_valid_;
};

/*
 * @test    fpga_reconf_slot_inv_len
 *
 * @details When the bitstream length is invalid, the function
 *          returns FPGA_INVALID_PARAM.
 */
TEST_P(reconf_c_hw_p, fpga_reconf_slot_inv_len) {
  fpga_result result;
  uint32_t slot = 0;
  int flags = 0;

  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_valid_.data(),
                                     -123456789, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);

  result = xfpga_fpgaReconfigureSlot(device_, slot, bitstream_valid_.data(),
                                     123456789, flags);
  EXPECT_EQ(result, FPGA_INVALID_PARAM);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(reconf_c_hw_p);
INSTANTIATE_TEST_SUITE_P(reconf, reconf_c_hw_p,
                         ::testing::ValuesIn(test_platform::hw_platforms({
                                                                           "dfl-d5005",
                                                                           "dfl-n3000",
                                                                           "dfl-n6000-sku0",
                                                                           "dfl-n6000-sku1"
                                                                         })));
