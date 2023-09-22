// Copyright(c) 2023, Intel Corporation
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

#include <ftw.h>
#include <errno.h>
#include <byteswap.h>
#include <uuid/uuid.h>
#include <ctype.h>

#include "gtest/gtest.h"
#include "mock/opae_std.h"
#include "mock/test_system.h"
using namespace opae::testing;

#include <opae/types_enum.h>
#include "cfg-file.h"
#include "props.h"

extern "C" {
#include "opae_vfio.h"

int read_file(const char *path, char *value, size_t max);
int read_pci_link(const char *addr, const char *link, char *value, size_t max);
int read_pci_attr(const char *addr, const char *attr, char *value, size_t max);
int read_pci_attr_u32(const char *addr, const char *attr, uint32_t *value);
int parse_pcie_info(vfio_pci_device_t *device, const char *addr);
void free_token_list(vfio_token *tokens);
vfio_pci_device_t *find_pci_device(const char addr[PCIADDR_MAX]);
vfio_pci_device_t *vfio_get_pci_device(const char addr[PCIADDR_MAX]);
bool pci_device_matches(const libopae_config_data *c,
                        uint16_t vid,
                        uint16_t did,
                        uint16_t svid,
                        uint16_t sdid);
bool pci_device_supported(const char *pcie_addr);
vfio_token *clone_token(vfio_token *src);
vfio_token *token_check(fpga_token token);
vfio_handle *handle_check(fpga_handle handle);
vfio_event_handle *event_handle_check(fpga_event_handle event_handle);
vfio_handle *handle_check_and_lock(fpga_handle handle);
vfio_event_handle *event_handle_check_and_lock(fpga_event_handle event_handle);

int close_vfio_pair(vfio_pair_t **pair);
fpga_result open_vfio_pair(const char *addr, vfio_pair_t **ppair);

fpga_result vfio_reset(const vfio_pci_device_t *dev,
                      volatile uint8_t *port_base);
int vfio_walk(vfio_pci_device_t *dev);

fpga_result vfio_fpgaOpen(fpga_token token, fpga_handle *handle, int flags);
fpga_result vfio_fpgaClose(fpga_handle handle);
fpga_result vfio_fpgaReset(fpga_handle handle);
fpga_result vfio_fpgaUpdateProperties(fpga_token token, fpga_properties prop);
fpga_result vfio_fpgaGetProperties(fpga_token token, fpga_properties *prop);
fpga_result vfio_fpgaGetPropertiesFromHandle(fpga_handle handle, fpga_properties *prop);
fpga_result vfio_fpgaWriteMMIO64(fpga_handle handle, uint32_t mmio_num,
                                uint64_t offset, uint64_t value);
fpga_result vfio_fpgaReadMMIO64(fpga_handle handle, uint32_t mmio_num,
                               uint64_t offset, uint64_t *value);
fpga_result vfio_fpgaWriteMMIO32(fpga_handle handle, uint32_t mmio_num,
                                uint64_t offset, uint32_t value);
fpga_result vfio_fpgaReadMMIO32(fpga_handle handle, uint32_t mmio_num,
                               uint64_t offset, uint32_t *value);
fpga_result vfio_fpgaWriteMMIO512(fpga_handle handle, uint32_t mmio_num,
                                 uint64_t offset, const void *value);
fpga_result vfio_fpgaMapMMIO(fpga_handle handle, uint32_t mmio_num,
                            uint64_t **mmio_ptr);
fpga_result vfio_fpgaUnmapMMIO(fpga_handle handle, uint32_t mmio_num);
vfio_token *find_token(const vfio_pci_device_t *dev, uint32_t region,
                      fpga_objtype objtype);
vfio_token *vfio_get_token(vfio_pci_device_t *dev, uint32_t region,
                         fpga_objtype objtype);

bool pci_matches_filter(const fpga_properties filter, vfio_pci_device_t *dev);
bool pci_matches_filters(const fpga_properties *filters,
                         uint32_t num_filters,
                         vfio_pci_device_t *dev);
bool matches_filter(const fpga_properties filter, vfio_token *t);
bool matches_filters(const fpga_properties *filters,
                     uint32_t num_filters,
                     vfio_token *t);
uint32_t vfio_irq_count(struct opae_vfio *device);

fpga_result vfio_fpgaEnumerate(const fpga_properties *filters,
                              uint32_t num_filters, fpga_token *tokens,
                              uint32_t max_tokens, uint32_t *num_matches);
fpga_result vfio_fpgaCloneToken(fpga_token src, fpga_token *dst);
fpga_result vfio_fpgaDestroyToken(fpga_token *token);
fpga_result vfio_fpgaPrepareBuffer(fpga_handle handle,
                                   uint64_t len,
                                   void **buf_addr,
                                   uint64_t *wsid,
                                   int flags);
fpga_result vfio_fpgaReleaseBuffer(fpga_handle handle, uint64_t wsid);
fpga_result vfio_fpgaGetIOAddress(fpga_handle handle,
                                  uint64_t wsid,
                                  uint64_t *ioaddr);

fpga_result vfio_fpgaCreateEventHandle(fpga_event_handle *event_handle);
fpga_result vfio_fpgaDestroyEventHandle(fpga_event_handle *event_handle);
fpga_result vfio_fpgaGetOSObjectFromEventHandle(const fpga_event_handle eh,
                                               int *fd);

fpga_result register_event(vfio_handle *_h, fpga_event_type event_type,
                           vfio_event_handle *_ueh, uint32_t flags);

fpga_result vfio_fpgaRegisterEvent(fpga_handle handle,
                                  fpga_event_type event_type,
                                  fpga_event_handle event_handle,
                                  uint32_t flags);

fpga_result unregister_event(vfio_handle *_h,
                             fpga_event_type event_type,
                             vfio_event_handle *_ueh);

fpga_result vfio_fpgaUnregisterEvent(fpga_handle handle,
                                    fpga_event_type event_type,
                                    fpga_event_handle event_handle);

static inline volatile uint8_t *get_user_offset(vfio_handle *h,
                                                uint32_t mmio_num,
                                                uint32_t offset)
{
        uint32_t user_mmio = h->token->user_mmio[mmio_num];

        return h->mmio_base + user_mmio + offset;
}

#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__) && GCC_VERSION >= 40900
static inline void copy512(const void *src, void *dst)
{
    asm volatile("vmovdqu64 (%0), %%zmm0;"
                 "vmovdqu64 %%zmm0, (%1);"
                 :
                 : "r"(src), "r"(dst));
}
#endif

extern vfio_pci_device_t *_pci_devices;
extern libopae_config_data *opae_v_supported_devices;

#define VFIO_TOKEN_MAGIC 0xEF1010FE
#define VFIO_HANDLE_MAGIC ~VFIO_TOKEN_MAGIC
#define VFIO_EVENT_HANDLE_MAGIC 0x5a6446a5
}

#define NLB0_GUID "D8424DC4-A4A3-C413-F89E-433683F9040B"

void get_valid_pci_addr(const char *attr_hint, char *addr, size_t max)
{
  char path[PATH_MAX];
  DIR *dir;
  struct dirent *dirent;

  dir = opae_opendir("/sys/bus/pci/devices");

  while ((dirent = readdir(dir)) != NULL) {
    if (!strcmp(dirent->d_name, ".") ||
        !strcmp(dirent->d_name, ".."))
      continue;

    snprintf(path, sizeof(path),
             "/sys/bus/pci/devices/%s/%s",
             dirent->d_name, attr_hint);

    struct stat st;
    memset(&st, 0, sizeof(st));

    opae_stat(path, &st);
    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
      strncpy(addr, dirent->d_name, max);
      break;
    }
  }

  opae_closedir(dir);
}

void set_pci_devices()
{
  _pci_devices = (vfio_pci_device_t *)opae_calloc(1, sizeof(vfio_pci_device_t));
  strcpy(_pci_devices->addr, "ffff:b1:00.0");

  _pci_devices->next = (vfio_pci_device_t *)opae_calloc(1, sizeof(vfio_pci_device_t));
  strcpy(_pci_devices->next->addr, "ffff:b2:00.0");

  _pci_devices->next->next = nullptr;

  _pci_devices->tokens = (vfio_token *)opae_calloc(1, sizeof(vfio_token));
  _pci_devices->tokens->hdr.magic = VFIO_TOKEN_MAGIC;
  _pci_devices->tokens->device = _pci_devices;
  _pci_devices->tokens->next = nullptr;

  _pci_devices->next->tokens = (vfio_token *)opae_calloc(1, sizeof(vfio_token));
  _pci_devices->next->tokens->hdr.magic = VFIO_TOKEN_MAGIC;
  _pci_devices->next->tokens->device = _pci_devices->next;
  _pci_devices->next->tokens->next = nullptr;
}

/**
 * @test    read_file_not_found
 * @brief   Test: read_file()
 * @details When the path argument to read_file()<br>
 *          does not exist, then read_file() returns<br>
 *          FPGA_EXCEPTION.
 */
TEST(opae_v, read_file_not_found)
{
  char tmpf[32];

  strcpy(tmpf, "tmpsys-XXXXXX.tmp");
  opae_close(mkstemps(tmpf, 4));
  unlink(tmpf);

  char value[32];
  EXPECT_EQ(FPGA_EXCEPTION, read_file(tmpf, value, sizeof(value)));
}

/**
 * @test    read_file_empty
 * @brief   Test: read_file()
 * @details When the path argument to read_file() exists<br>
 *          but is empty, then read_file() returns<br>
 *          FPGA_EXCEPTION.
 */
TEST(opae_v, read_file_empty)
{
  char tmpf[32];

  strcpy(tmpf, "tmpsys-XXXXXX.tmp");
  opae_close(mkstemps(tmpf, 4));

  char value[32];
  EXPECT_EQ(FPGA_EXCEPTION, read_file(tmpf, value, sizeof(value)));

  unlink(tmpf);
}

/**
 * @test    read_file_ok
 * @brief   Test: read_file()
 * @details When the path argument to read_file() exists<br>
 *          and is non-empty, then read_file() returns<br>
 *          FPGA_OK after reading the file.
 */
TEST(opae_v, read_file_ok)
{
  char tmpf[32];

  strcpy(tmpf, "tmpsys-XXXXXX.tmp");
  int fd = mkstemps(tmpf, 4);
  write(fd, "ou812", 5);
  opae_close(fd);

  char value[32];
  memset(value, 0, sizeof(value));
  EXPECT_EQ(FPGA_OK, read_file(tmpf, value, sizeof(value)));
  EXPECT_STREQ("ou812", value);

  unlink(tmpf);
}

/**
 * @test    read_pci_attr_err
 * @brief   Test: read_pci_attr()
 * @details When the addr parameter is invalid,<br>
 *          the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, read_pci_attr_err)
{
  const char *addr = "doesnt-exist";
  const char *attr = "device";
  char value[32];
  EXPECT_EQ(FPGA_EXCEPTION, read_pci_attr(addr, attr, value, sizeof(value)));
}

/**
 * @test    read_pci_attr_u32_no_file
 * @brief   Test: read_pci_attr_u32()
 * @details When the addr parameter is invalid,<br>
 *          the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, read_pci_attr_u32_no_file)
{
  const char *addr = "doesnt-exist";
  const char *attr = "device";
  uint32_t value = 0;
  EXPECT_EQ(FPGA_EXCEPTION, read_pci_attr_u32(addr, attr, &value));
}

/**
 * @test    read_pci_attr_u32_not_int
 * @brief   Test: read_pci_attr_u32()
 * @details When the addr parameter is valid, but<br>
 *          the given attr does not contain an unsigned<br>
 *          integer value, then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, read_pci_attr_u32_not_int)
{
  char addr[32] = { 0, };
  const char *attr_hint = "power_state";

  get_valid_pci_addr(attr_hint, addr, sizeof(addr));

  ASSERT_NE('\0', addr[0]);

  uint32_t value = 0;
  EXPECT_EQ(FPGA_EXCEPTION, read_pci_attr_u32(addr, attr_hint, &value));
  EXPECT_EQ(0, value);
}

/**
 * @test    read_pci_attr_u32_ok
 * @brief   Test: read_pci_attr_u32()
 * @details When the addr parameter is valid, and<br>
 *          the given attr does contain an unsigned<br>
 *          integer value, then the function returns FPGA_OK<br>
 *          after retrieving the value.
 */
TEST(opae_v, read_pci_attr_u32_ok)
{
  char addr[32] = { 0, };
  const char *attr_hint = "device";

  get_valid_pci_addr(attr_hint, addr, sizeof(addr));

  ASSERT_NE('\0', addr[0]);

  uint32_t value = 0;
  EXPECT_EQ(FPGA_OK, read_pci_attr_u32(addr, attr_hint, &value));
  EXPECT_NE(0, value);
}

/**
 * @test    parse_pcie_info_err0
 * @brief   Test: parse_pcie_info()
 * @details When the addr parameter is not a valid<br>
 *          PCIe address, the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, parse_pcie_info_err0)
{
  vfio_pci_device_t vfio_dev;
  memset(&vfio_dev, 0, sizeof(vfio_dev));

  EXPECT_EQ(FPGA_EXCEPTION, parse_pcie_info(&vfio_dev, "invalid_device_address"));
}

/**
 * @test    parse_pcie_info_ok
 * @brief   Test: parse_pcie_info()
 * @details When the addr parameter is a valid<br>
 *          PCIe address, the function returns FPGA_OK.
 */
TEST(opae_v, parse_pcie_info_ok)
{
  vfio_pci_device_t vfio_dev;
  memset(&vfio_dev, 0, sizeof(vfio_dev));

  EXPECT_EQ(FPGA_OK, parse_pcie_info(&vfio_dev, "1111:FF:1F.4"));
  EXPECT_EQ(0x1111, vfio_dev.bdf.segment);
  EXPECT_EQ(0xFF, vfio_dev.bdf.bus);
  EXPECT_EQ(0x1F, vfio_dev.bdf.device);
  EXPECT_EQ(4, vfio_dev.bdf.function);
}

/**
 * @test    free_token_list_ok
 * @brief   Test: free_token_list()
 * @details free_token_list() frees the entire list of<br>
 *          token objects.
 */
TEST(opae_v, free_token_list_ok)
{
  vfio_token *tok = (vfio_token *)opae_malloc(sizeof(vfio_token));
  tok->next = (vfio_token *)opae_malloc(sizeof(vfio_token));
  tok->next->next = nullptr;

  free_token_list(tok);
}

/**
 * @test    vfio_free_device_list_ok
 * @brief   Test: vfio_free_device_list()
 * @details vfio_free_device_list() frees the entire list of<br>
 *          vfio_pci_device_t objects.
 */
TEST(opae_v, vfio_free_device_list_ok)
{
  set_pci_devices();
  vfio_free_device_list();
  EXPECT_EQ(nullptr, _pci_devices);
}

/**
 * @test    find_pci_device_ok
 * @brief   Test: find_pci_device()
 * @details find_pci_device() retrieves the correct<br>
 *          entry per the search criteria.
 */
TEST(opae_v, find_pci_device_ok)
{
  set_pci_devices();
  const char *addr = "ffff:b2:00.0";

  vfio_pci_device_t *dev = find_pci_device(addr);
  ASSERT_NE(nullptr, dev);

  EXPECT_STREQ(dev->addr, addr);

  vfio_free_device_list();
}

/**
 * @test    find_pci_device_not_found
 * @brief   Test: find_pci_device()
 * @details find_pci_device() returns NULL when<br>
 *          no match is found.
 */
TEST(opae_v, find_pci_device_not_found)
{
  set_pci_devices();
  const char *addr = "0000:ab:00.0";

  vfio_pci_device_t *dev = find_pci_device(addr);
  EXPECT_EQ(nullptr, dev);

  vfio_free_device_list();
}

/**
 * @test    vfio_get_pci_device_found
 * @brief   Test: vfio_get_pci_device()
 * @details vfio_get_pci_device() returns a pointer to the<br>
 *          appropriate device when the device is found<br>
 *          in _pci_devices.
 */
TEST(opae_v, vfio_get_pci_device_found)
{
  set_pci_devices();
  const char *addr = "ffff:b1:00.0";

  vfio_pci_device_t *dev = vfio_get_pci_device(addr);
  ASSERT_NE(nullptr, dev);

  EXPECT_STREQ(addr, dev->addr);

  vfio_free_device_list();
}

/**
 * @test    vfio_get_pci_device_calloc_err
 * @brief   Test: vfio_get_pci_device()
 * @details vfio_get_pci_device() returns a NULL<br>
 *          when the device is not found and calloc fails.
 */
TEST(opae_v, vfio_get_pci_device_calloc_err)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  set_pci_devices();
  const char *addr = "0000:ab:00.0";

  test_system::instance()->invalidate_calloc(0, "vfio_get_pci_device");

  vfio_pci_device_t *dev = vfio_get_pci_device(addr);
  EXPECT_EQ(nullptr, dev);

  vfio_free_device_list();
}

/**
 * @test    vfio_get_pci_device_address_err
 * @brief   Test: vfio_get_pci_device()
 * @details vfio_get_pci_device() returns NULL<br>
 *          when the given address is invalid.
 */
TEST(opae_v, vfio_get_pci_device_address_err)
{
  set_pci_devices();
  const char *addr = "doesnt_exist";

  vfio_pci_device_t *dev = vfio_get_pci_device(addr);
  EXPECT_EQ(nullptr, dev);

  vfio_free_device_list();
}

/**
 * @test    vfio_get_pci_device_ok
 * @brief   Test: vfio_get_pci_device()
 * @details vfio_get_pci_device() returns a new<br>
 *          vfio_pci_device_t * when successful.
 */
TEST(opae_v, vfio_get_pci_device_ok)
{
  set_pci_devices();
  char addr[32] = { 0, };

  get_valid_pci_addr("vendor", addr, sizeof(addr));
  EXPECT_NE('\0', addr[0]);

  vfio_pci_device_t *dev = vfio_get_pci_device(addr);
  ASSERT_NE(nullptr, dev);

  EXPECT_STREQ(addr, dev->addr);
  EXPECT_NE(0, dev->vendor);
  EXPECT_NE(0, dev->device);

  vfio_free_device_list();
}

/**
 * @test    pci_device_matches_err0
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          module_library field is not libopae-v.so<br>
 *          the function returns false.
 */
TEST(opae_v, pci_device_matches_err0)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libxfpga.so";
  c.vendor_id = vid;
  c.device_id = did;
  c.subsystem_vendor_id = svid;
  c.subsystem_device_id = sdid;

  EXPECT_EQ(false, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_matches_err1
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          vendor_id or device_id field does not<br>
 *          match the given parameters, then the<br>
 *          function returns false.
 */
TEST(opae_v, pci_device_matches_err1)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libopae-v.so";
  c.vendor_id = vid + 1;
  c.device_id = did + 1;
  c.subsystem_vendor_id = svid;
  c.subsystem_device_id = sdid;

  EXPECT_EQ(false, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_matches_err2
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          subsystem_vendor_id field does not<br>
 *          match the given parameter and is not<br>
 *          OPAE_VENDOR_ANY, then the function<br>
 *          returns false.
 */
TEST(opae_v, pci_device_matches_err2)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libopae-v.so";
  c.vendor_id = vid;
  c.device_id = did;
  c.subsystem_vendor_id = svid + 1;
  c.subsystem_device_id = sdid;

  EXPECT_EQ(false, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_matches_err3
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          subsystem_device_id field does not<br>
 *          match the given parameter and is not<br>
 *          OPAE_DEVICE_ANY, then the function<br>
 *          returns false.
 */
TEST(opae_v, pci_device_matches_err3)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libopae-v.so";
  c.vendor_id = vid;
  c.device_id = did;
  c.subsystem_vendor_id = svid;
  c.subsystem_device_id = sdid + 1;

  EXPECT_EQ(false, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_matches_ok0
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          fields match the given parameters,<br>
 *          then the function returns true.
 */
TEST(opae_v, pci_device_matches_ok0)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libopae-v.so";
  c.vendor_id = vid;
  c.device_id = did;
  c.subsystem_vendor_id = svid;
  c.subsystem_device_id = sdid;

  EXPECT_EQ(true, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_matches_ok1
 * @brief   Test: pci_device_matches()
 * @details When the given config data's<br>
 *          fields match the given parameters,<br>
 *          including the wildcards OPAE_VENDOR_ANY<br>
 *          and OPAE_DEVICE_ANY, then the function<br>
 *          returns true.
 */
TEST(opae_v, pci_device_matches_ok1)
{
  const uint16_t vid = 0x8086;
  const uint16_t did = 0xbcce;
  const uint16_t svid = 0x8086;
  const uint16_t sdid = 0x138d;

  libopae_config_data c;
  c.module_library = "libopae-v.so";
  c.vendor_id = vid;
  c.device_id = did;
  c.subsystem_vendor_id = OPAE_VENDOR_ANY;
  c.subsystem_device_id = OPAE_DEVICE_ANY;

  EXPECT_EQ(true, pci_device_matches(&c, vid, did, svid, sdid));
}

/**
 * @test    pci_device_supported_err0
 * @brief   Test: pci_device_supported()
 * @details When the PCI device attributes for the<br>
 *          given pcie_addr parameter cannot be read,<br>
 *          then the function returns false.
 */
TEST(opae_v, pci_device_supported_err0)
{
  const char *dev = "doesnt_exist";
  EXPECT_EQ(false, pci_device_supported(dev));
}

/**
 * @test    pci_device_supported_err1
 * @brief   Test: pci_device_supported()
 * @details When the device ID 4-tuple is not found<br>
 *          in the configuration table stored at<br>
 *          opae_v_supported_devices, then the<br>
 *          function returns false.
 */
TEST(opae_v, pci_device_supported_err1)
{
  char addr[32] = { 0, };
  get_valid_pci_addr("vendor", addr, sizeof(addr));
  ASSERT_NE('\0', addr[0]);

  uint32_t vid = 0;
  uint32_t did = 0;
  uint32_t svid = 0;
  uint32_t sdid = 0;

  ASSERT_EQ(0, read_pci_attr_u32(addr, "vendor", &vid));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "device", &did));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "subsystem_vendor", &svid));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "subsystem_device", &sdid));

  libopae_config_data c[2];
  c[0].module_library = "libopae-v.so";
  c[0].vendor_id = (uint16_t)vid + 1; // force not found
  c[0].device_id = (uint16_t)did;
  c[0].subsystem_vendor_id = (uint16_t)svid;
  c[0].subsystem_device_id = (uint16_t)sdid;
  c[1].module_library = NULL;

  opae_v_supported_devices = c;
  EXPECT_EQ(false, pci_device_supported(addr));
  opae_v_supported_devices = nullptr;
}

/**
 * @test    pci_device_supported_ok
 * @brief   Test: pci_device_supported()
 * @details When the device ID 4-tuple is found<br>
 *          in the configuration table stored at<br>
 *          opae_v_supported_devices, then the<br>
 *          function returns true.
 */
TEST(opae_v, pci_device_supported_ok)
{
  char addr[32] = { 0, };
  get_valid_pci_addr("vendor", addr, sizeof(addr));
  ASSERT_NE('\0', addr[0]);

  uint32_t vid = 0;
  uint32_t did = 0;
  uint32_t svid = 0;
  uint32_t sdid = 0;

  ASSERT_EQ(0, read_pci_attr_u32(addr, "vendor", &vid));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "device", &did));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "subsystem_vendor", &svid));
  ASSERT_EQ(0, read_pci_attr_u32(addr, "subsystem_device", &sdid));

  libopae_config_data c[2];
  c[0].module_library = "libopae-v.so";
  c[0].vendor_id = (uint16_t)vid;
  c[0].device_id = (uint16_t)did;
  c[0].subsystem_vendor_id = (uint16_t)svid;
  c[0].subsystem_device_id = (uint16_t)sdid;
  c[1].module_library = NULL;

  opae_v_supported_devices = c;
  EXPECT_EQ(true, pci_device_supported(addr));
  opae_v_supported_devices = nullptr;
}

int write_file(const char *path, const char *mode, const char *data, size_t sz)
{
  FILE *fp = fopen(path, mode);
  if (fp) {
    fwrite(data, 1, sz, fp);
    fclose(fp);
    return 0;
  }
  return 1;
}

int remove_ftw_callback(const char *fpath,
                        const struct stat *sb,
                        int typeflag,
                        struct FTW *ftwbuf)
{
  UNUSED_PARAM(sb);
  UNUSED_PARAM(typeflag);
  UNUSED_PARAM(ftwbuf);
  return remove(fpath);
}

class vfio_pci_discover_f : public ::testing::Test
{
  protected:

  vfio_pci_discover_f()
  : pwd_(nullptr)
  {}

  virtual void SetUp() override
  {
    pwd_ = getcwd(nullptr, 0);
    memset(tmpd_, 0, sizeof(tmpd_));

    pcie_addr_[0] = '\0';
    get_valid_pci_addr("vendor", pcie_addr_, sizeof(pcie_addr_));
    ASSERT_NE('\0', pcie_addr_[0]);

    snprintf(tmpd_, sizeof(tmpd_),
             "%s/tmpdir-XXXXXX", pwd_);
    ASSERT_NE(nullptr, mkdtemp(tmpd_));

    char *p = tmpd_ + strlen(tmpd_);

    const int dperms = 0700;

    // sys/bus/pci/drivers/vfio-pci/????:??:??.?
    strcat(tmpd_, "/sys");
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    strcat(tmpd_, "/bus");
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    strcat(tmpd_, "/pci");
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    strcat(tmpd_, "/drivers");
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    strcat(tmpd_, "/vfio-pci");
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    strcat(tmpd_, "/");
    strcat(tmpd_, pcie_addr_);
    ASSERT_NE(-1, mkdir(tmpd_, dperms));

    *p = '\0';
  }

  virtual void TearDown() override
  {
    const int flags = FTW_DEPTH | FTW_MOUNT | FTW_PHYS;
    EXPECT_NE(-1, nftw(tmpd_, remove_ftw_callback, FOPEN_MAX, flags));
    opae_free(pwd_);
  }

  char *pwd_;
  char tmpd_[PATH_MAX];
  char pcie_addr_[32];
};

/**
 * @test    vfio_pci_discover_ok0
 * @brief   Test: vfio_pci_discover()
 * @details When the gpattern parameter to<br>
 *          vfio_pci_discover() results in no matches,<br>
 *          the the function returns 0.
 */
TEST_F(vfio_pci_discover_f, vfio_pci_discover_ok0)
{
  char gpattern[PATH_MAX];
  size_t len = strnlen(tmpd_, PATH_MAX);

  memcpy(gpattern, tmpd_, len + 1);
  strcat(gpattern, "/doesnt_exist");

  EXPECT_EQ(0, vfio_pci_discover(gpattern));
}

/**
 * @test    vfio_pci_discover_err4
 * @brief   Test: vfio_pci_discover()
 * @details When the gpattern parameter to<br>
 *          vfio_pci_discover() describes a device<br>
 *          that is not supported according to the<br>
 *          configuration data, then that match is<br>
 *          skipped.
 */
TEST_F(vfio_pci_discover_f, vfio_pci_discover_err4)
{
  uint32_t vid = 0;
  uint32_t did = 0;
  uint32_t svid = 0;
  uint32_t sdid = 0;

  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "vendor", &vid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "device", &did));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_vendor", &svid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_device", &sdid));

  libopae_config_data c[2];
  c[0].module_library = "libopae-v.so";
  c[0].vendor_id = (uint16_t)vid + 1; // force a mismatch
  c[0].device_id = (uint16_t)did;
  c[0].subsystem_vendor_id = (uint16_t)svid;
  c[0].subsystem_device_id = (uint16_t)sdid;
  c[1].module_library = NULL;

  opae_v_supported_devices = c;

  char gpattern[PATH_MAX];

  size_t len = strnlen(tmpd_, PATH_MAX);

  memcpy(gpattern, tmpd_, len + 1);
  strcat(gpattern, "/sys/bus/pci/drivers/vfio-pci/????:??:??.?");

  EXPECT_NE(0, vfio_pci_discover(gpattern));
  opae_v_supported_devices = nullptr;
}

/**
 * @test    vfio_pci_discover_err5
 * @brief   Test: vfio_pci_discover()
 * @details When the gpattern parameter to<br>
 *          vfio_pci_discover() describes a device<br>
 *          that is supported according to the<br>
 *          configuration data, but calloc fails,<br>
 *          then the function returns non-zero.
 */
TEST_F(vfio_pci_discover_f, vfio_pci_discover_err5)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  uint32_t vid = 0;
  uint32_t did = 0;
  uint32_t svid = 0;
  uint32_t sdid = 0;

  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "vendor", &vid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "device", &did));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_vendor", &svid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_device", &sdid));

  libopae_config_data c[2];
  c[0].module_library = "libopae-v.so";
  c[0].vendor_id = (uint16_t)vid;
  c[0].device_id = (uint16_t)did;
  c[0].subsystem_vendor_id = (uint16_t)svid;
  c[0].subsystem_device_id = (uint16_t)sdid;
  c[1].module_library = NULL;

  opae_v_supported_devices = c;

  char gpattern[PATH_MAX];

  size_t len = strnlen(tmpd_, PATH_MAX);

  memcpy(gpattern, tmpd_, len + 1);
  strcat(gpattern, "/sys/bus/pci/drivers/vfio-pci/????:??:??.?");

  test_system::instance()->invalidate_calloc(0, "vfio_get_pci_device");

  EXPECT_NE(0, vfio_pci_discover(gpattern));
  opae_v_supported_devices = nullptr;
}

/**
 * @test    vfio_pci_discover_ok1
 * @brief   Test: vfio_pci_discover()
 * @details When the gpattern parameter to<br>
 *          vfio_pci_discover() results in at least<br>
 *          one successful match,<br>
 *          then the function returns 0.
 */
TEST_F(vfio_pci_discover_f, vfio_pci_discover_ok1)
{
  uint32_t vid = 0;
  uint32_t did = 0;
  uint32_t svid = 0;
  uint32_t sdid = 0;

  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "vendor", &vid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "device", &did));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_vendor", &svid));
  ASSERT_EQ(0, read_pci_attr_u32(pcie_addr_, "subsystem_device", &sdid));

  libopae_config_data c[2];
  c[0].module_library = "libopae-v.so";
  c[0].vendor_id = (uint16_t)vid;
  c[0].device_id = (uint16_t)did;
  c[0].subsystem_vendor_id = (uint16_t)svid;
  c[0].subsystem_device_id = (uint16_t)sdid;
  c[1].module_library = NULL;

  opae_v_supported_devices = c;

  char gpattern[PATH_MAX];

  size_t len = strnlen(tmpd_, PATH_MAX);

  memcpy(gpattern, tmpd_, len + 1);
  strcat(gpattern, "/sys/bus/pci/drivers/vfio-pci/????:??:??.?");

  EXPECT_EQ(0, vfio_pci_discover(gpattern));

  opae_v_supported_devices = nullptr;
  vfio_free_device_list();
}

/**
 * @test    clone_token_err0
 * @brief   Test: clone_token()
 * @details When the src parameter to<br>
 *          clone_token() does not contain<br>
 *          the proper token magic,<br>
 *          then the function returns NULL.
 */
TEST(opae_v, clone_token_err0)
{
  vfio_token t;
  t.hdr.magic = ~VFIO_TOKEN_MAGIC;

  EXPECT_EQ(nullptr, clone_token(&t));
}

/**
 * @test    clone_token_err1
 * @brief   Test: clone_token()
 * @details When malloc() fails,<br>
 *          the function returns NULL.
 */
TEST(opae_v, clone_token_err1)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_token t;
  t.hdr.magic = VFIO_TOKEN_MAGIC;

  test_system::instance()->invalidate_malloc(0, "clone_token");

  EXPECT_EQ(nullptr, clone_token(&t));
}

/**
 * @test    clone_token_ok
 * @brief   Test: clone_token()
 * @details When successful,<br>
 *          the function returns a copy<br>
 *          of the input token.
 */
TEST(opae_v, clone_token_ok)
{
  vfio_token parent;
  memset(&parent, 0, sizeof(parent));
  parent.hdr.magic = VFIO_TOKEN_MAGIC;
  parent.hdr.bus = 0xf5;

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.hdr.bus = 0x5f;
  t.parent = &parent;

  vfio_token *copy = clone_token(&t);
  ASSERT_NE(nullptr, copy);

  EXPECT_EQ(0x5f, copy->hdr.bus);

  vfio_token *par = copy->parent;
  ASSERT_NE(nullptr, par);

  EXPECT_EQ(0xf5, par->hdr.bus);

  opae_free(par);
  opae_free(copy);
}

/**
 * @test    token_check_err0
 * @brief   Test: token_check()
 * @details When the given fpga_token,<br>
 *          is NULL, then the fuction<br>
 *          returns NULL.
 */
TEST(opae_v, token_check_err0)
{
  EXPECT_EQ(nullptr, token_check(nullptr));
}

/**
 * @test    token_check_err1
 * @brief   Test: token_check()
 * @details When the given fpga_token<br>
 *          does not contain the correct<br>
 *          token magic<br>
 *          then the function returns NULL.
 */
TEST(opae_v, token_check_err1)
{
  vfio_token t;
  t.hdr.magic = ~VFIO_TOKEN_MAGIC;

  EXPECT_EQ(nullptr, token_check(&t));
}

/**
 * @test    token_check_ok
 * @brief   Test: token_check()
 * @details When the given fpga_token<br>
 *          is ok, then the function returns<br>
 *          a corresponding vfio_token *.
 */
TEST(opae_v, token_check_ok)
{
  vfio_token t;
  t.hdr.magic = VFIO_TOKEN_MAGIC;

  vfio_token *p = token_check(&t);
  EXPECT_EQ(&t, p);
}

/**
 * @test    handle_check_err0
 * @brief   Test: handle_check()
 * @details When the given fpga_handle<br>
 *          is NULL, then the fuction<br>
 *          returns NULL.
 */
TEST(opae_v, handle_check_err0)
{
  EXPECT_EQ(nullptr, handle_check(nullptr));
}

/**
 * @test    handle_check_err1
 * @brief   Test: handle_check()
 * @details When the given fpga_handle<br>
 *          does not contain the correct<br>
 *          handle magic<br>
 *          then the function returns NULL.
 */
TEST(opae_v, handle_check_err1)
{
  vfio_handle h;
  h.magic = ~VFIO_HANDLE_MAGIC;

  EXPECT_EQ(nullptr, handle_check(&h));
}

/**
 * @test    handle_check_ok
 * @brief   Test: handle_check()
 * @details When the given fpga_handle<br>
 *          is ok, then the function returns<br>
 *          a corresponding vfio_handle *.
 */
TEST(opae_v, handle_check_ok)
{
  vfio_handle h;
  h.magic = VFIO_HANDLE_MAGIC;

  vfio_handle *p = handle_check(&h);
  EXPECT_EQ(&h, p);
}

/**
 * @test    event_handle_check_err0
 * @brief   Test: event_handle_check()
 * @details When the given fpga_event_handle<br>
 *          is NULL, then the fuction<br>
 *          returns NULL.
 */
TEST(opae_v, event_handle_check_err0)
{
  EXPECT_EQ(nullptr, event_handle_check(nullptr));
}

/**
 * @test    event_handle_check_err1
 * @brief   Test: event_handle_check()
 * @details When the given fpga_event_handle<br>
 *          does not contain the correct<br>
 *          event handle magic<br>
 *          then the function returns NULL.
 */
TEST(opae_v, event_handle_check_err1)
{
  vfio_event_handle eh;
  eh.magic = ~VFIO_EVENT_HANDLE_MAGIC;

  EXPECT_EQ(nullptr, event_handle_check(&eh));
}

/**
 * @test    event_handle_check_ok
 * @brief   Test: event_handle_check()
 * @details When the given fpga_event_handle<br>
 *          is ok, then the function returns<br>
 *          a corresponding vfio_event_handle *.
 */
TEST(opae_v, event_handle_check_ok)
{
  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;

  vfio_event_handle *p = event_handle_check(&eh);
  EXPECT_EQ(&eh, p);
}

/**
 * @test    handle_check_and_lock_err
 * @brief   Test: handle_check_and_lock()
 * @details When the given fpga_handle<br>
 *          is invalid, then the function<br>
 *          returns NULL.<br>
 */
TEST(opae_v, handle_check_and_lock_err)
{
  EXPECT_EQ(nullptr, handle_check_and_lock(nullptr));
}

/**
 * @test    handle_check_and_lock_ok
 * @brief   Test: handle_check_and_lock()
 * @details When the given fpga_handle<br>
 *          is valid, then the function<br>
 *          locks the mutex and returns<br>
 *          a vfio_handle *.
 */
TEST(opae_v, handle_check_and_lock_ok)
{
  vfio_handle h;
  h.magic = VFIO_HANDLE_MAGIC;
  h.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  vfio_handle *p = handle_check_and_lock(&h);
  ASSERT_EQ(&h, p);
  EXPECT_EQ(0, pthread_mutex_unlock(&h.lock));
}

/**
 * @test    event_handle_check_and_lock_err
 * @brief   Test: event_handle_check_and_lock()
 * @details When the given fpga_event_handle<br>
 *          is invalid, then the function<br>
 *          returns NULL.<br>
 */
TEST(opae_v, event_handle_check_and_lock_err)
{
  EXPECT_EQ(nullptr, event_handle_check_and_lock(nullptr));
}

/**
 * @test    event_handle_check_and_lock_ok
 * @brief   Test: event_handle_check_and_lock()
 * @details When the given fpga_event_handle<br>
 *          is valid, then the function<br>
 *          locks the mutex and returns<br>
 *          a vfio_event_handle *.
 */
TEST(opae_v, event_handle_check_and_lock_ok)
{
  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  vfio_event_handle *p = event_handle_check_and_lock(&eh);
  ASSERT_EQ(&eh, p);
  EXPECT_EQ(0, pthread_mutex_unlock(&eh.lock));
}

/**
 * @test    close_vfio_pair_ok
 * @brief   Test: close_vfio_pair()
 * @details When the given parameters are<br>
 *          non-NULL, then the function returns 0.
 */
TEST(opae_v, close_vfio_pair_ok)
{
  vfio_pair_t *pair = (vfio_pair_t *)opae_malloc(sizeof(*pair));

  struct opae_vfio *device = (struct opae_vfio *)opae_calloc(1, sizeof(*device));
  device->group.group_fd = -1;
  device->device.device_fd = -1;
  device->cont_fd = -1;
  mem_alloc_init(&device->iova_alloc);

  struct opae_vfio *physfn = (struct opae_vfio *)opae_calloc(1, sizeof(*physfn));
  physfn->group.group_fd = -1;
  physfn->device.device_fd = -1;
  physfn->cont_fd = -1;
  mem_alloc_init(&physfn->iova_alloc);

  pair->device = device;
  pair->physfn = physfn;

  EXPECT_EQ(0, close_vfio_pair(&pair));
}

/**
 * @test    open_vfio_pair_err0
 * @brief   Test: open_vfio_pair()
 * @details When the first call to malloc() fails,<br>
 *          then the function returns FPGA_NO_MEMORY.
 */
TEST(opae_v, open_vfio_pair_err0)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_pair_t *pair = nullptr;
  const char *addr = "0000:00:00.0";

  test_system::instance()->invalidate_malloc(0, "open_vfio_pair");

  EXPECT_EQ(FPGA_NO_MEMORY, open_vfio_pair(addr, &pair));
}

/**
 * @test    open_vfio_pair_err1
 * @brief   Test: open_vfio_pair()
 * @details When the second call to malloc() fails,<br>
 *          then the function returns FPGA_NO_MEMORY.
 */
TEST(opae_v, open_vfio_pair_err1)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_pair_t *pair = nullptr;
  const char *addr = "0000:00:00.0";

  test_system::instance()->invalidate_malloc(1, "open_vfio_pair");

  EXPECT_EQ(FPGA_NO_MEMORY, open_vfio_pair(addr, &pair));
}

/**
 * @test    vfio_reset
 * @brief   Test: vfio_reset()
 * @details When the given parameters are<br>
 *          non-NULL, then the function returns<br>
 *          FPGA_OK.
 */
TEST(opae_v, vfio_reset)
{
  vfio_pci_device_t d;
  memset(&d, 0, sizeof(d));
  uint8_t mmio[4096];
  volatile uint8_t *port_base = (volatile uint8_t *)mmio;

  EXPECT_EQ(FPGA_OK, vfio_reset(&d, port_base));
}

/**
 * @test    vfio_walk_err0
 * @brief   Test: vfio_walk()
 * @details When the given dfl_device<br>
 *          doesn't exist, then the function<br>
 *          returns non-zero.
 */
TEST(opae_v, vfio_walk_err0)
{
  vfio_pci_device_t d;
  memset(&d, 0, sizeof(d));

  memcpy(d.addr, "none", 5);

  EXPECT_NE(0, vfio_walk(&d));
}

/**
 * @test    vfio_fpgaOpen_err0
 * @brief   Test: vfio_fpgaOpen()
 * @details When calloc() fails,<br>
 *          then the function<br>
 *          returns FPGA_NO_MEMORY.
 */
TEST(opae_v, vfio_fpgaOpen_err0)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;

  fpga_handle h;

  test_system::instance()->invalidate_calloc(0, "vfio_fpgaOpen");

  EXPECT_EQ(FPGA_NO_MEMORY, vfio_fpgaOpen(&t, &h, FPGA_OPEN_SHARED));
}

/**
 * @test    vfio_fpgaOpen_err1
 * @brief   Test: vfio_fpgaOpen()
 * @details When opae_vio_open() fails,<br>
 *          then the function<br>
 *          returns non-zero.
 */
TEST(opae_v, vfio_fpgaOpen_err1)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));
  memcpy(device.addr, "none", 5);

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.device = &device;

  fpga_handle handle = nullptr;

  EXPECT_NE(FPGA_OK, vfio_fpgaOpen(&t, &handle, 0));
}

/**
 * @test    vfio_fpgaClose_no_token_ok
 * @brief   Test: vfio_fpgaClose()
 * @details When the given handle,<br>
 *          has no corresponding token,<br>
 *          but is otherwise valid,<br>
 *          then the function returns FPGA_OK.
 */
TEST(opae_v, vfio_fpgaClose_no_token_ok)
{
  vfio_handle *h = (vfio_handle *)opae_calloc(1, sizeof(*h));
  h->magic = VFIO_HANDLE_MAGIC;
  h->lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  //h->uio.device_fd = -1;

  EXPECT_EQ(FPGA_OK, vfio_fpgaClose(h));
}

/**
 * @test    vfio_fpgaClose_ok
 * @brief   Test: vfio_fpgaClose()
 * @details When the given handle,<br>
 *          has a token, then that token<br>
 *          object and any associated parent<br>
 *          is freed, and the function returns FPGA_OK.
 */
TEST(opae_v, vfio_fpgaClose_ok)
{
  vfio_token *parent = (vfio_token *)opae_calloc(1, sizeof(*parent));
  parent->hdr.magic = VFIO_TOKEN_MAGIC;

  vfio_token *t = (vfio_token *)opae_calloc(1, sizeof(*t));
  t->hdr.magic = VFIO_TOKEN_MAGIC;
  t->parent = parent;

  vfio_handle *h = (vfio_handle *)opae_calloc(1, sizeof(*h));
  h->magic = VFIO_HANDLE_MAGIC;
  h->lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  //h->uio.device_fd = -1;
  h->token = t;

  EXPECT_EQ(FPGA_OK, vfio_fpgaClose(h));
}

fpga_result test_reset(const vfio_pci_device_t *p, volatile uint8_t *mmio)
{
  UNUSED_PARAM(p);
  UNUSED_PARAM(mmio);
  return FPGA_OK;
}

/**
 * @test    vfio_fpgaReset_ok
 * @brief   Test: vfio_fpgaReset()
 * @details When the given handle has<br>
 *          an FPGA_ACCELERATOR token,<br>
 *          and that token's ops.reset is non-NULL,<br>
 *          then the return value from that ops.reset<br>
 *          function is propagated as the return<br>
 *          result of vfio_fpgaReset().
 */
TEST(opae_v, vfio_fpgaReset_ok)
{
  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.hdr.objtype = FPGA_ACCELERATOR;
  t.ops.reset = test_reset;

  vfio_handle h;
  memset(&h, 0, sizeof(h));
  h.magic = VFIO_HANDLE_MAGIC;
  h.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  h.token = &t;

  EXPECT_EQ(FPGA_OK, vfio_fpgaReset(&h));
}

/**
 * @test    vfio_get_guid_ok
 * @brief   Test: vfio_get_guid()
 * @details When the given mmio pointer<br>
 *          is non-NULL, then the function<br>
 *          correctly transforms the guid<br>
 *          from its hardware-based format,<br>
 *          returning FPGA_OK.
 */
TEST(opae_v, vfio_get_guid_ok)
{
  fpga_guid tmp;
  fpga_guid hw_guid;

  ASSERT_EQ(0, uuid_parse(NLB0_GUID, tmp));

  uint64_t *src = (uint64_t *)tmp;
  uint64_t *dst = (uint64_t *)hw_guid;

  *dst = bswap_64(*(src+1));
  *(dst+1) = bswap_64(*src);

  fpga_guid transformed;
  EXPECT_EQ(FPGA_OK, vfio_get_guid((uint64_t *)hw_guid, transformed));

  char buf[GUIDSTR_MAX];
  uuid_unparse(transformed, buf);
  for (char *p = buf ; *p ; ++p) {
    *p = toupper(*p);
  }

  EXPECT_STREQ(NLB0_GUID, buf);
}

/**
 * @test    vfio_fpgaUpdateProperties_err0
 * @brief   Test: vfio_fpgaUpdateProperties()
 * @details When the given fpga_properties<br>
 *          pointer is NULL, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, vfio_fpgaUpdateProperties_err0)
{
  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaUpdateProperties(&t, nullptr));
}

/**
 * @test    vfio_fpgaUpdateProperties_ok
 * @brief   Test: vfio_fpgaUpdateProperties()
 * @details When the given fpga_properties<br>
 *          pointer has an invalid magic field,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, vfio_fpgaUpdateProperties_ok)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));
  device.bdf.bus = 0xa5;

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.hdr.objtype = FPGA_ACCELERATOR;
  t.device = &device;

  struct _fpga_properties p;
  memset(&p, 0, sizeof(p));
  p.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  p.magic = FPGA_PROPERTY_MAGIC;

  EXPECT_EQ(FPGA_OK, vfio_fpgaUpdateProperties(&t, &p));
  EXPECT_EQ(0xa5, p.bus);

  t.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_OK, vfio_fpgaUpdateProperties(&t, &p));
}

/**
 * @test    vfio_fpgaGetProperties_err0
 * @brief   Test: vfio_fpgaUpdateProperties()
 * @details When calloc() fails,<br>
 *          then the function returns FPGA_NO_MEMORY.
 */
TEST(opae_v, vfio_fpgaGetProperties_err0)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  fpga_properties props = nullptr;

  test_system::instance()->invalidate_calloc(0, "opae_properties_create");

  EXPECT_EQ(FPGA_NO_MEMORY, vfio_fpgaGetProperties(nullptr, &props));
}

/**
 * @test    vfio_fpgaGetProperties_err1
 * @brief   Test: vfio_fpgaUpdateProperties()
 * @details When the input token is invalid,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, vfio_fpgaGetProperties_err1)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = ~VFIO_TOKEN_MAGIC;
  t.device = &device;

  fpga_properties props = nullptr;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaGetProperties(&t, &props));
}

/**
 * @test    vfio_fpgaGetProperties_ok
 * @brief   Test: vfio_fpgaUpdateProperties()
 * @details When the parameters are valid,<br>
 *          then the function retrieves the properties<br>
 *          and returns FPGA_OK.
 */
TEST(opae_v, vfio_fpgaGetProperties_ok)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));
  device.bdf.bus = 0xa5;

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.device = &device;

  fpga_properties props = nullptr;
  struct _fpga_properties *_props;

  EXPECT_EQ(FPGA_OK, vfio_fpgaGetProperties(&t, &props));

  _props = (struct _fpga_properties *)props;
  EXPECT_EQ(0xa5, _props->bus);

  EXPECT_EQ(0, pthread_mutex_destroy(&_props->lock));
  opae_free(_props);
}

/**
 * @test    vfio_fpgaGetPropertiesFromHandle_err0
 * @brief   Test: vfio_fpgaUpdatePropertiesFromHandle()
 * @details When the handle parameter contains a NULL<br>
 *          token, then the function returns<br>
 *          FPGA_INVALID_PARAM.
 */
TEST(opae_v, vfio_fpgaGetPropertiesFromHandle_err0)
{
  vfio_handle h;
  memset(&h, 0, sizeof(h));
  h.magic = VFIO_HANDLE_MAGIC;
  h.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
 
  fpga_properties props = nullptr;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaGetPropertiesFromHandle(&h, &props));
}

/**
 * @test    vfio_fpgaGetPropertiesFromHandle_ok
 * @brief   Test: vfio_fpgaUpdatePropertiesFromHandle()
 * @details When the parameters are valid,<br>
 *          then the function retrieves the properties<br>
 *          and returns FPGA_OK.
 */
TEST(opae_v, vfio_fpgaGetPropertiesFromHandle_ok)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));
  device.bdf.bus = 0xa5;
  memcpy(device.addr, "none", 5);

  vfio_token t;
  memset(&t, 0, sizeof(t));
  t.hdr.magic = VFIO_TOKEN_MAGIC;
  t.hdr.objtype = FPGA_ACCELERATOR;
  t.device = &device;

  vfio_handle h;
  memset(&h, 0, sizeof(h));
  h.magic = VFIO_HANDLE_MAGIC;
  h.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  h.token = &t;

  fpga_properties props = nullptr;

  EXPECT_EQ(FPGA_OK, vfio_fpgaGetPropertiesFromHandle(&h, &props));
  
  struct _fpga_properties *_props = (struct _fpga_properties *)props;
  EXPECT_EQ(0xa5, _props->bus);

  EXPECT_EQ(0, pthread_mutex_destroy(&_props->lock));
  opae_free(_props);
}

/**
 * @test    get_user_offset_ok
 * @brief   Test: get_user_offset()
 * @details The function returns the correct MMIO<br>
 *          address per the parameters.<br>
 */
TEST(opae_v, get_user_offset_ok)
{
  int i;
  vfio_token t;

  for (i = 0 ; i < USER_MMIO_MAX ; ++i)
    t.user_mmio[i] = i;

  volatile uint8_t *mmio_base = (volatile uint8_t *)0xdeadbeef;

  vfio_handle h;
  h.token = &t;
  h.mmio_base = mmio_base;

  for (i = 0 ; i < USER_MMIO_MAX ; ++i) {
    EXPECT_EQ(mmio_base + t.user_mmio[i], get_user_offset(&h, i, 0));
  }
}

class vfio_mmio_f : public ::testing::Test
{
  protected:

  vfio_mmio_f()
  {}

  virtual void SetUp() override
  {
    memset(&device_, 0, sizeof(device_));

    memset(&token_, 0, sizeof(token_));
    token_.hdr.magic = VFIO_TOKEN_MAGIC;
    token_.hdr.objtype = FPGA_ACCELERATOR;
    token_.user_mmio_count = 1;
    token_.user_mmio[0] = 0;
    token_.device = &device_;

    memset(mmio_, 0, sizeof(mmio_));

    handle_.magic = VFIO_HANDLE_MAGIC;
    handle_.token = &token_;
    handle_.mmio_base = mmio_;
    handle_.mmio_size = sizeof(mmio_);
    handle_.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__)
#if GCC_VERSION >= 40900
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512f")) {
      handle_.flags |= OPAE_FLAG_HAS_AVX512;
    }
#endif // GCC_VERSION
#endif // x86
  }

  virtual void TearDown() override
  {
    handle_.flags = 0;
  }

  vfio_pci_device_t device_;
  vfio_token token_;
  uint8_t mmio_[4096];
  vfio_handle handle_;
};

/**
 * @test    vfio_fpgaWriteMMIO64_err0
 * @brief   Test: vfio_fpgaWriteMMIO64()
 * @details When the objtype field of the token<br>
 *          header is FPGA_DEVICE, then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO64_err0)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint64_t value = 0xdeadbeefc0cac01a;
  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaWriteMMIO64(&handle_, mmio_num, offset, value));
}

/**
 * @test    vfio_fpgaWriteMMIO64_err1
 * @brief   Test: vfio_fpgaWriteMMIO64()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO64_err1)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  const uint64_t offset = 0;
  const uint64_t value = 0xdeadbeefc0cac01a;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaWriteMMIO64(&handle_, mmio_num, offset, value));
}

/**
 * @test    vfio_fpgaWriteMMIO64_ok
 * @brief   Test: vfio_fpgaWriteMMIO64()
 * @details When the parameters are valid,<br>
 *          then the function performs the write<br>
 *          and returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO64_ok)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint64_t value = 0xdeadbeefc0cac01a;
  EXPECT_EQ(FPGA_OK, vfio_fpgaWriteMMIO64(&handle_, mmio_num, offset, value));

  uint64_t *p = (uint64_t *)mmio_;
  EXPECT_EQ(value, *p);
}

/**
 * @test    vfio_fpgaReadMMIO64_err0
 * @brief   Test: vfio_fpgaReadMMIO64()
 * @details When the objtype field of the token<br>
 *          header is FPGA_DEVICE, then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO64_err0)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  uint64_t value = 0;

  uint64_t *p = (uint64_t *)mmio_;
  const uint64_t rvalue = 0xdecafbadfeedbeef;
  *p = rvalue;

  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaReadMMIO64(&handle_, mmio_num, offset, &value));
}

/**
 * @test    vfio_fpgaReadMMIO64_err1
 * @brief   Test: vfio_fpgaReadMMIO64()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO64_err1)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  const uint64_t offset = 0;
  uint64_t value = 0;

  uint64_t *p = (uint64_t *)mmio_;
  const uint64_t rvalue = 0xdecafbadfeedbeef;
  *p = rvalue;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaReadMMIO64(&handle_, mmio_num, offset, &value));
}

/**
 * @test    vfio_fpgaReadMMIO64_ok
 * @brief   Test: vfio_fpgaReadMMIO64()
 * @details When the parameters are valid,<br>
 *          then the function performs the read<br>
 *          and returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO64_ok)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  uint64_t value = 0;

  uint64_t *p = (uint64_t *)mmio_;
  const uint64_t rvalue = 0xdecafbadfeedbeef;
  *p = rvalue;

  EXPECT_EQ(FPGA_OK, vfio_fpgaReadMMIO64(&handle_, mmio_num, offset, &value));
  EXPECT_EQ(rvalue, value);
}

/**
 * @test    vfio_fpgaWriteMMIO32_err0
 * @brief   Test: vfio_fpgaWriteMMIO32()
 * @details When the objtype field of the token<br>
 *          header is FPGA_DEVICE, then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO32_err0)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint32_t value = 0xc0cac01a;
  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaWriteMMIO32(&handle_, mmio_num, offset, value));
}

/**
 * @test    vfio_fpgaWriteMMIO32_err1
 * @brief   Test: vfio_fpgaWriteMMIO32()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO32_err1)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  const uint64_t offset = 0;
  const uint32_t value = 0xc0cac01a;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaWriteMMIO32(&handle_, mmio_num, offset, value));
}

/**
 * @test    vfio_fpgaWriteMMIO32_ok
 * @brief   Test: vfio_fpgaWriteMMIO32()
 * @details When the parameters are valid,<br>
 *          then the function performs the write<br>
 *          and returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO32_ok)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint32_t value = 0xc0cac01a;
  EXPECT_EQ(FPGA_OK, vfio_fpgaWriteMMIO32(&handle_, mmio_num, offset, value));

  uint32_t *p = (uint32_t *)mmio_;
  EXPECT_EQ(value, *p);
}

/**
 * @test    vfio_fpgaReadMMIO32_err0
 * @brief   Test: vfio_fpgaReadMMIO32()
 * @details When the objtype field of the token<br>
 *          header is FPGA_DEVICE, then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO32_err0)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  uint32_t value = 0;

  uint32_t *p = (uint32_t *)mmio_;
  const uint32_t rvalue = 0xdecafbad;
  *p = rvalue;

  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaReadMMIO32(&handle_, mmio_num, offset, &value));
}

/**
 * @test    vfio_fpgaReadMMIO32_err1
 * @brief   Test: vfio_fpgaReadMMIO32()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO32_err1)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  const uint64_t offset = 0;
  uint32_t value = 0;

  uint32_t *p = (uint32_t *)mmio_;
  const uint32_t rvalue = 0xdecafbad;
  *p = rvalue;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaReadMMIO32(&handle_, mmio_num, offset, &value));
}

/**
 * @test    vfio_fpgaReadMMIO32_ok
 * @brief   Test: vfio_fpgaReadMMIO32()
 * @details When the parameters are valid,<br>
 *          then the function performs the read<br>
 *          and returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaReadMMIO32_ok)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  uint32_t value = 0;

  uint32_t *p = (uint32_t *)mmio_;
  const uint32_t rvalue = 0xdecafbad;
  *p = rvalue;

  EXPECT_EQ(FPGA_OK, vfio_fpgaReadMMIO32(&handle_, mmio_num, offset, &value));
  EXPECT_EQ(rvalue, value);
}

#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__) && GCC_VERSION >= 40900
/**
 * @test    copy512_ok
 * @brief   Test: copy512()
 * @details The function copies the 64 bytes at src
 *          to dst.
 */
TEST(opae_v, copy512_ok)
{
  __builtin_cpu_init();
  if (!__builtin_cpu_supports("avx512f")) {
    GTEST_SKIP() << "CPU doesn't have AVX512 support.";
  }

  struct
  {
    uint64_t header;
    uint8_t between[64];
    uint64_t footer;
  } src, dst;

  src.header = 0xdecafbaddeadbeef;
  memset(src.between, 0xaf, sizeof(src.between));
  src.footer = 0xbeefdeadbaddecaf;

  memset(&dst, 0, sizeof(dst));

  copy512(src.between, dst.between);

  EXPECT_EQ(0, dst.header);
  EXPECT_EQ(0, dst.footer);
  EXPECT_EQ(0, memcmp(src.between, dst.between, sizeof(src.between)));
}
#else
TEST(opae_v, copy512_ok)
{
  GTEST_SKIP() << "Invalid platform or gcc too old.";
}
#endif

/**
 * @test    vfio_fpgaWriteMMIO512_err0
 * @brief   Test: vfio_fpgaWriteMMIO512()
 * @details When the offset parameter is not<br>
 *          aligned to a 64-byte boundary,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO512_err0)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 1; // <- invalid offset
  const uint64_t values[8] = {
    0x0000000100000001, 0x0000000200000002, 0x0000000300000003, 0x0000000400000004,
    0x0000000500000005, 0x0000000600000006, 0x0000000700000007, 0x0000000800000008
  };
  
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaWriteMMIO512(&handle_, mmio_num, offset, values));
}

/**
 * @test    vfio_fpgaWriteMMIO512_err1
 * @brief   Test: vfio_fpgaWriteMMIO512()
 * @details When the objtype field of the token<br>
 *          header is FPGA_DEVICE, then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO512_err1)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint64_t values[8] = {
    0x0000000100000001, 0x0000000200000002, 0x0000000300000003, 0x0000000400000004,
    0x0000000500000005, 0x0000000600000006, 0x0000000700000007, 0x0000000800000008
  };

  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaWriteMMIO512(&handle_, mmio_num, offset, values));
}

/**
 * @test    vfio_fpgaWriteMMIO512_err2
 * @brief   Test: vfio_fpgaWriteMMIO512()
 * @details When the flags field of the handle<br>
 *          doesn't contain OPAE_FLAG_HAS_AVX512,<br>
 *          then the function<br>
 *          returns FPGA_NOT_SUPPORTED.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO512_err2)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint64_t values[8] = {
    0x0000000100000001, 0x0000000200000002, 0x0000000300000003, 0x0000000400000004,
    0x0000000500000005, 0x0000000600000006, 0x0000000700000007, 0x0000000800000008
  };

  handle_.flags = 0;
  EXPECT_EQ(FPGA_NOT_SUPPORTED, vfio_fpgaWriteMMIO512(&handle_, mmio_num, offset, values));
}

/**
 * @test    vfio_fpgaWriteMMIO512_err3
 * @brief   Test: vfio_fpgaWriteMMIO512()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO512_err3)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  const uint64_t offset = 0;
  const uint64_t values[8] = {
    0x0000000100000001, 0x0000000200000002, 0x0000000300000003, 0x0000000400000004,
    0x0000000500000005, 0x0000000600000006, 0x0000000700000007, 0x0000000800000008
  };

  handle_.flags |= OPAE_FLAG_HAS_AVX512;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaWriteMMIO512(&handle_, mmio_num, offset, values));
}

/**
 * @test    vfio_fpgaWriteMMIO512_ok
 * @brief   Test: vfio_fpgaWriteMMIO512()
 * @details When the parameters are valid,<br>
 *          then the function copies the 64<br>
 *          bytes from value to the destination<br>
 *          location in the mmio,<br>
 *          and the function returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaWriteMMIO512_ok)
{
  const uint32_t mmio_num = 0;
  const uint64_t offset = 0;
  const uint64_t values[8] = {
    0x0000000100000001, 0x0000000200000002, 0x0000000300000003, 0x0000000400000004,
    0x0000000500000005, 0x0000000600000006, 0x0000000700000007, 0x0000000800000008
  };

  if (!(handle_.flags & OPAE_FLAG_HAS_AVX512)) {
    GTEST_SKIP() << "CPU doesn't have AVX512 support.";
  }

  EXPECT_EQ(FPGA_OK, vfio_fpgaWriteMMIO512(&handle_, mmio_num, offset, values));
  EXPECT_EQ(0, memcmp(values, mmio_, sizeof(values)));
}

/**
 * @test    vfio_fpgaMapMMIO_err0
 * @brief   Test: vfio_fpgaMapMMIO()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaMapMMIO_err0)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  uint64_t *mmio_ptr = nullptr;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaMapMMIO(&handle_, mmio_num, &mmio_ptr));
}

/**
 * @test    vfio_fpgaMapMMIO_ok
 * @brief   Test: vfio_fpgaMapMMIO()
 * @details When the parameters are ok,<br>
 *          then the function copies the<br>
 *          MMIO pointer into mmio_ptr and<br>
 *          returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaMapMMIO_ok)
{
  const uint32_t mmio_num = 0;
  uint64_t *mmio_ptr = nullptr;

  EXPECT_EQ(FPGA_OK, vfio_fpgaMapMMIO(&handle_, mmio_num, &mmio_ptr));
  EXPECT_EQ(mmio_ptr, (uint64_t *)mmio_);
}

/**
 * @test    vfio_fpgaUnmapMMIO_err0
 * @brief   Test: vfio_fpgaUnmapMMIO()
 * @details When the given mmio_num is out of<br>
 *          bounds, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST_F(vfio_mmio_f, vfio_fpgaUnmapMMIO_err0)
{
  const uint32_t mmio_num = USER_MMIO_MAX; // <- out of bounds
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaUnmapMMIO(&handle_, mmio_num));
}

/**
 * @test    vfio_fpgaUnmapMMIO_ok
 * @brief   Test: vfio_fpgaUnmapMMIO()
 * @details When the parameters are ok,<br>
 *          then the function<br>
 *          returns FPGA_OK.
 */
TEST_F(vfio_mmio_f, vfio_fpgaUnmapMMIO_ok)
{
  const uint32_t mmio_num = 0;
  EXPECT_EQ(FPGA_OK, vfio_fpgaUnmapMMIO(&handle_, mmio_num));
}

/**
 * @test    find_token_err0
 * @brief   Test: find_token()
 * @details When no token matching the search<br>
 *          criteria is found,<br>
 *          then the function returns NULL.
 */
TEST(opae_v, find_token_err0)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;
  token.device = &device;
  token.region = 1;
  token.hdr.objtype = FPGA_DEVICE;

  device.tokens = &token;

  EXPECT_EQ(nullptr, find_token(&device, 0, FPGA_ACCELERATOR));
}

/**
 * @test    find_token_ok
 * @brief   Test: find_token()
 * @details When a token matching the search<br>
 *          criteria is found,<br>
 *          then the function returns its address.
 */
TEST(opae_v, find_token_ok)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;
  token.device = &device;
  token.region = 1;
  token.hdr.objtype = FPGA_DEVICE;

  device.tokens = &token;

  EXPECT_EQ(&token, find_token(&device, 1, FPGA_DEVICE));
}

/**
 * @test    vfio_get_token_ok0
 * @brief   Test: vfio_get_token()
 * @details When a token matching the search<br>
 *          criteria is found,<br>
 *          then the function returns<br>
 *          a pointer to the token.
 */
TEST(opae_v, vfio_get_token_ok0)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;
  token.device = &device;
  token.region = 1;
  token.hdr.objtype = FPGA_DEVICE;

  device.tokens = &token;

  EXPECT_EQ(&token, vfio_get_token(&device, 1, FPGA_DEVICE));
}

/**
 * @test    vfio_get_token_err0
 * @brief   Test: vfio_get_token()
 * @details When no token matching the search<br>
 *          criteria is found,<br>
 *          and when calloc() fails,<br>
 *          then the function returns NULL.
 */
TEST(opae_v, vfio_get_token_err0)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;
  token.device = &device;
  token.region = 1;
  token.hdr.objtype = FPGA_DEVICE;

  device.tokens = &token;

  test_system::instance()->invalidate_calloc(0, "vfio_get_token");

  EXPECT_EQ(nullptr, vfio_get_token(&device, 0, FPGA_ACCELERATOR));
}

/**
 * @test    vfio_get_token_ok1
 * @brief   Test: vfio_get_token()
 * @details When no token matching the search<br>
 *          criteria is found,<br>
 *          then the function returns<br>
 *          a new token.
 */
TEST(opae_v, vfio_get_token_ok1)
{
  vfio_pci_device_t device;
  memset(&device, 0, sizeof(device));

  vfio_token *token = vfio_get_token(&device, 1, FPGA_ACCELERATOR);
  ASSERT_NE(nullptr, token);

  EXPECT_EQ(device.tokens, token);
  EXPECT_EQ(nullptr, token->next);
  EXPECT_EQ(VFIO_TOKEN_MAGIC, token->hdr.magic);
  EXPECT_EQ(&device, token->device);
  EXPECT_EQ(1, token->region);
  EXPECT_EQ(FPGA_ACCELERATOR, token->hdr.objtype);

  opae_free(token);
}

/**
 * @test    pci_matches_filter
 * @brief   Test: pci_matches_filter()
 * @details When any of the fields in the<br>
 *          given device struct is not a match<br>
 *          for the properties in the filter<br>
 *          struct, then the function return false.
 */
TEST(opae_v, pci_matches_filter)
{
  struct _fpga_properties _p;

  _p.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  _p.magic = FPGA_PROPERTY_MAGIC;
  _p.valid_fields = 0;

  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SEGMENT);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_BUS);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_DEVICE);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_FUNCTION);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SOCKETID);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_VENDORID);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_DEVICEID);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SUB_VENDORID);
  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SUB_DEVICEID);

  vfio_pci_device_t dev;

  const uint16_t segment = 0xbeef;
  const uint8_t bus = 0x5a;
  const uint8_t device = 4;
  const uint8_t function = 3;
  const uint8_t socket_id = 2;
  const uint16_t vendor_id = 0x8086;
  const uint16_t device_id = 0x1234;
  const uint16_t sub_vendor_id = 0x8086;
  const uint16_t sub_device_id = 0x1234;

  _p.segment = segment;
  dev.bdf.segment = segment;

  _p.bus = bus;
  dev.bdf.bus = bus;

  _p.device = device;
  dev.bdf.device = device;

  _p.function = function;
  dev.bdf.function = function;

  _p.socket_id = socket_id;
  dev.numa_node = socket_id;

  _p.vendor_id = vendor_id;
  dev.vendor = vendor_id;

  _p.device_id = device_id;
  dev.device = device_id;

  _p.subsystem_vendor_id = sub_vendor_id;
  dev.subsystem_vendor = sub_vendor_id;

  _p.subsystem_device_id = sub_device_id;
  dev.subsystem_device = sub_device_id;

  EXPECT_EQ(true, pci_matches_filter(&_p, &dev));

  dev.bdf.segment = segment + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.bdf.segment = segment;

  dev.bdf.bus = bus + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.bdf.bus = bus;

  dev.bdf.device = device + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.bdf.device = device;

  dev.bdf.function = function + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.bdf.function = function;

  dev.numa_node = socket_id + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.numa_node = socket_id;

  dev.vendor = vendor_id + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.vendor = vendor_id;

  dev.device = device_id + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.device = device_id;

  dev.subsystem_vendor = sub_vendor_id + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.subsystem_vendor = sub_vendor_id;

  dev.subsystem_device = sub_device_id + 1;
  EXPECT_EQ(false, pci_matches_filter(&_p, &dev));
  dev.subsystem_device = sub_device_id;
}

/**
 * @test    pci_matches_filters_empty
 * @brief   Test: pci_matches_filters()
 * @details When the given filters pointer<br>
 *          is NULL, signifying no match criteria,<br>
 *          then the function returns true.
 */
TEST(opae_v, pci_matches_filters_empty)
{
  vfio_pci_device_t dev;
  EXPECT_EQ(true, pci_matches_filters(nullptr, 0, &dev));
}

/**
 * @test    pci_matches_filters_ok
 * @brief   Test: pci_matches_filters()
 * @details When the given filters pointer<br>
 *          is non-NULL, then the function<br>
 *          returns true when the given device<br>
 *          matches.
 */
TEST(opae_v, pci_matches_filters_ok)
{
  struct _fpga_properties _p;

  _p.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  _p.magic = FPGA_PROPERTY_MAGIC;
  _p.valid_fields = 0;

  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SEGMENT);

  vfio_pci_device_t dev;

  const uint16_t segment = 0xbeef;
 
  _p.segment = segment;
  dev.bdf.segment = segment;

  fpga_properties filters[] = { &_p };
  const uint32_t num_filters = 1;

  EXPECT_EQ(true, pci_matches_filters(filters, num_filters, &dev));
}

/**
 * @test    pci_matches_filters_mismatch
 * @brief   Test: pci_matches_filters()
 * @details When the given filters pointer<br>
 *          is non-NULL, then the function<br>
 *          returns false when none of the given<br>
 *          devices match.
 */
TEST(opae_v, pci_matches_filters_mismatch)
{
  struct _fpga_properties _p;

  _p.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  _p.magic = FPGA_PROPERTY_MAGIC;
  _p.valid_fields = 0;

  SET_FIELD_VALID(&_p, FPGA_PROPERTY_SEGMENT);

  vfio_pci_device_t dev;

  const uint16_t segment = 0xbeef;
 
  _p.segment = segment + 1; // <- force mismatch
  dev.bdf.segment = segment;

  fpga_properties filters[] = { &_p };
  const uint32_t num_filters = 1;

  EXPECT_EQ(false, pci_matches_filters(filters, num_filters, &dev));
}

/**
 * @test    vfio_irq_count_not_found
 * @brief   Test: vfio_irq_count()
 * @details When the given struct opae_vfio<br>
 *          has no entry that matches VFIO_PCI_MSIX_IRQ_INDEX,<br>
 *          then the function returns 0.
 */
TEST(opae_v, vfio_irq_count_not_found)
{
  struct opae_vfio v;
  memset(&v, 0, sizeof(v));

  EXPECT_EQ(0, vfio_irq_count(&v));
}

/**
 * @test    vfio_irq_count_ok
 * @brief   Test: vfio_irq_count()
 * @details When the given struct opae_vfio<br>
 *          has no entry that matches VFIO_PCI_MSIX_IRQ_INDEX,<br>
 *          then the function returns 0.
 */
TEST(opae_v, vfio_irq_count_ok)
{
  struct opae_vfio_device_irq irq0;
  memset(&irq0, 0, sizeof(irq0));
  irq0.count = 2;
  irq0.index = VFIO_PCI_MSIX_IRQ_INDEX + 1;

  struct opae_vfio_device_irq irq1;
  memset(&irq1, 0, sizeof(irq1));
  irq1.count = 3;
  irq1.index = VFIO_PCI_MSIX_IRQ_INDEX;

  irq0.next = &irq1;

  struct opae_vfio v;
  memset(&v, 0, sizeof(v));
  v.device.irqs = &irq0;

  EXPECT_EQ(3, vfio_irq_count(&v));
}

class matches_filter_f : public ::testing::Test
{
 protected:
  matches_filter_f()
  {}

  virtual void SetUp() override
  {
    memset(&props_, 0, sizeof(props_));

    props_.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    props_.magic = FPGA_PROPERTY_MAGIC;
    props_.valid_fields = 0;

    memset(&token_, 0, sizeof(token_));
    memset(&parent_, 0, sizeof(parent_));

    token_.hdr.magic = VFIO_TOKEN_MAGIC;
    parent_.hdr.magic = VFIO_TOKEN_MAGIC;

    token_.hdr.segment = 0xbeef;
    token_.hdr.bus = 0xa5;
    token_.hdr.device = 7;
    token_.hdr.function = 3;

    parent_.hdr.segment = 0xbeef;
    parent_.hdr.bus = 0xa5;
    parent_.hdr.device = 7;
    parent_.hdr.function = 3;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_PARENT);
    props_.parent = &parent_;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_OBJTYPE);
    props_.objtype = FPGA_ACCELERATOR;
    token_.hdr.objtype = FPGA_ACCELERATOR;
    parent_.hdr.objtype = FPGA_DEVICE;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_ACCELERATOR_STATE);
    props_.u.accelerator.state = FPGA_ACCELERATOR_UNASSIGNED;
    token_.afu_state = FPGA_ACCELERATOR_UNASSIGNED;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_NUM_INTERRUPTS);
    props_.u.accelerator.num_interrupts = 1;
    token_.num_afu_irqs = 1;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_OBJECTID);
    props_.object_id = 0x1f00;
    token_.hdr.object_id = 0x1f00;

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_GUID);
    EXPECT_EQ(0, uuid_parse(NLB0_GUID, props_.guid));
    EXPECT_EQ(0, uuid_parse(NLB0_GUID, token_.hdr.guid));
    EXPECT_EQ(0, uuid_parse(NLB0_GUID, token_.compat_id));

    SET_FIELD_VALID(&props_, FPGA_PROPERTY_INTERFACE);
    props_.interface = FPGA_IFC_VFIO;
  }

  struct _fpga_properties props_;
  vfio_token token_;
  vfio_token parent_;
};

/**
 * @test    matches_filter_parent_err0
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that a parent value is set,<br>
 *          but that parent value is NULL,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_parent_err0)
{
  props_.parent = nullptr;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_parent_err1
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that a parent value is set,<br>
 *          and that parent value is valid,<br>
 *          but the parent field does not correspond<br>
 *          to a parent of the given token,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_parent_err1)
{
  parent_.hdr.bus = 0xa6;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_objtype_err2
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the objtype value is set,<br>
 *          but the objtype field does not match that<br>
 *          of the given token,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_objtype_err2)
{
  token_.hdr.objtype = FPGA_DEVICE;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_state_err3
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the accelerator state value is set,<br>
 *          but the accelerator state field does not match that<br>
 *          of the given token,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_state_err3)
{
  token_.afu_state = FPGA_ACCELERATOR_ASSIGNED;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_irqs_err4
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the num irqs value is set,<br>
 *          but the num irqs field does not match that<br>
 *          of the given token,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_irqs_err4)
{
  token_.num_afu_irqs = 2;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_object_id_err5
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the object ID value is set,<br>
 *          but the object ID field does not match that<br>
 *          of the given token,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_object_id_err5)
{
  token_.hdr.object_id = 0xf100;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_guid_err6
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the object GUID value is set,<br>
 *          and the objtype is FPGA_ACCELERATOR,<br>
 *          but the GUID in the properties object does not match that<br>
 *          of the given token's header,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_guid_err6)
{
  memset(token_.hdr.guid, 0, sizeof(token_.hdr.guid));
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_guid_err7
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the object GUID value is set,<br>
 *          and the objtype is FPGA_DEVICE,<br>
 *          but the GUID in the properties object does not match<br>
 *          the given token's compat_id,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_guid_err7)
{
  CLEAR_FIELD_VALID(&props_, FPGA_PROPERTY_PARENT);
  CLEAR_FIELD_VALID(&props_, FPGA_PROPERTY_OBJTYPE);
  token_.hdr.objtype = FPGA_DEVICE;
  memset(token_.compat_id, 0, sizeof(token_.compat_id));
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_interface_err8
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          indicates that the interface value is set,<br>
 *          but the interface in the properties object<br>
 *          is not FPGA_IFC_VFIO,<br>
 *          then the function returns false.
 */
TEST_F(matches_filter_f, matches_filter_interface_err8)
{
  props_.interface = FPGA_IFC_DFL;
  EXPECT_EQ(false, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filter_ok
 * @brief   Test: matches_filter()
 * @details When the given properties object<br>
 *          matches the properties of the given token,<br>
 *          then the function returns true.
 */
TEST_F(matches_filter_f, matches_filter_ok)
{
  EXPECT_EQ(true, matches_filter(&props_, &token_));
}

/**
 * @test    matches_filters_ok0
 * @brief   Test: matches_filters()
 * @details When the given filters array is NULL,<br>
 *          signifying no match criteria,<br>
 *          then the function returns true.
 */
TEST(opae_v, matches_filters_ok0)
{
  vfio_token token;
  EXPECT_EQ(true, matches_filters(NULL, 0, &token));
}

/**
 * @test    matches_filters_ok1
 * @brief   Test: matches_filters()
 * @details When any of the given filters objects matches,<br>
 *          the properties of the given token,<br>
 *          then the function returns true.
 */
TEST(opae_v, matches_filters_ok1)
{
  struct _fpga_properties props;
  memset(&props, 0, sizeof(props));

  props.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  props.magic = FPGA_PROPERTY_MAGIC;
  props.valid_fields = 0;

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;

  SET_FIELD_VALID(&props, FPGA_PROPERTY_OBJTYPE);
  props.objtype = FPGA_ACCELERATOR;
  token.hdr.objtype = FPGA_ACCELERATOR;

  fpga_properties filters[] = { &props };
  const uint32_t num_filters = 1;

  EXPECT_EQ(true, matches_filters(filters, num_filters, &token));
}

/**
 * @test    matches_filters_mismatch
 * @brief   Test: matches_filters()
 * @details When none of the given filters objects matches,<br>
 *          the properties of the given token,<br>
 *          then the function returns false.
 */
TEST(opae_v, matches_filters_mismatch)
{
  struct _fpga_properties props;
  memset(&props, 0, sizeof(props));

  props.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  props.magic = FPGA_PROPERTY_MAGIC;
  props.valid_fields = 0;

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;

  SET_FIELD_VALID(&props, FPGA_PROPERTY_OBJTYPE);
  props.objtype = FPGA_ACCELERATOR;
  token.hdr.objtype = FPGA_DEVICE; // <- force mismatch

  fpga_properties filters[] = { &props };
  const uint32_t num_filters = 1;

  EXPECT_EQ(false, matches_filters(filters, num_filters, &token));
}

/**
 * @test    enum_ok0
 * @brief   Test: vfio_fpgaEnumerate()
 * @details When the _pci_devices list is empty,<br>
 *          then the output num_matches will be 0,<br>
 *          and function returns FPGA_OK.
 */
TEST(opae_v, enum_ok0)
{
  _pci_devices = nullptr;
  fpga_token tokens[2] = { nullptr, nullptr };
  uint32_t num_matches = 0;

  EXPECT_EQ(FPGA_OK, vfio_fpgaEnumerate(nullptr, 0, tokens, 2, &num_matches));
  EXPECT_EQ(0, num_matches);
}

/**
 * @test    enum_ok1
 * @brief   Test: vfio_fpgaEnumerate()
 * @details When the _pci_devices list is non-empty,<br>
 *          then the output num_matches will match the<br>
 *          number of tokens retrieved,<br>
 *          and the function returns FPGA_OK.
 */
TEST(opae_v, enum_ok1)
{
  set_pci_devices();

  fpga_token tokens[2] = { nullptr, nullptr };
  uint32_t num_matches = 0;

  EXPECT_EQ(FPGA_OK, vfio_fpgaEnumerate(nullptr, 0, tokens, 2, &num_matches));
  EXPECT_EQ(2, num_matches);

  ASSERT_NE(nullptr, tokens[0]);
  ASSERT_NE(nullptr, tokens[1]);

  opae_free(tokens[0]);
  opae_free(tokens[1]);
  vfio_free_device_list();
}

/**
 * @test    clone_err0
 * @brief   Test: vfio_fpgaCloneToken()
 * @details When either the src or the dst input,<br>
 *          pointer is NULL,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, clone_err0)
{
  vfio_token token;
  fpga_token dst = nullptr;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaCloneToken(&token, nullptr));
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaCloneToken(nullptr, &dst));
}

/**
 * @test    clone_err1
 * @brief   Test: vfio_fpgaCloneToken()
 * @details When the src token has an invalid magic field,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, clone_err1)
{
  vfio_token token;
  memset(&token, 0, sizeof(token));
  fpga_token dst = nullptr;

  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaCloneToken(&token, &dst));
}

/**
 * @test    clone_err2
 * @brief   Test: vfio_fpgaCloneToken()
 * @details When malloc() fails,<br>
 *          then the function returns FPGA_NO_MEMORY.
 */
TEST(opae_v, clone_err2)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK

  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;

  fpga_token dst = nullptr;

  test_system::instance()->invalidate_malloc(0, "vfio_fpgaCloneToken");

  EXPECT_EQ(FPGA_NO_MEMORY, vfio_fpgaCloneToken(&token, &dst));
}

/**
 * @test    clone_ok
 * @brief   Test: vfio_fpgaCloneToken()
 * @details When succesful,<br>
 *          the function creates a copy of the given token,<br>
 *          then the function returns FPGA_OK.
 */
TEST(opae_v, clone_ok)
{
  vfio_token token;
  memset(&token, 0, sizeof(token));
  token.hdr.magic = VFIO_TOKEN_MAGIC;
  token.hdr.bus = 0xa5;

  fpga_token dst = nullptr;

  EXPECT_EQ(FPGA_OK, vfio_fpgaCloneToken(&token, &dst));
  ASSERT_NE(nullptr, dst);

  vfio_token *p = (vfio_token *)dst;
  EXPECT_EQ(VFIO_TOKEN_MAGIC, p->hdr.magic);
  EXPECT_EQ(0xa5, p->hdr.bus);

  opae_free(p);
}

/**
 * @test    destroy_err0
 * @brief   Test: vfio_fpgaDestroyToken()
 * @details When either the input token pointer<br>
 *          or its de-referenced value is NULL,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, destroy_err0)
{
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyToken(nullptr));

  fpga_token token = nullptr;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyToken(&token));
}

/**
 * @test    destroy_err1
 * @brief   Test: vfio_fpgaDestroyToken()
 * @details When the input token's magic field<br>
 *          is invalid,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, destroy_err1)
{
  vfio_token token;
  memset(&token, 0, sizeof(token));

  fpga_token t = &token;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyToken(&t));
}

/**
 * @test    destroy_ok
 * @brief   Test: vfio_fpgaDestroyToken()
 * @details When the input token is valid<br>
 *          then the function frees it and<br>
 *          returns FPGA_OK.
 */
TEST(opae_v, destroy_ok)
{
  vfio_token *token = (vfio_token *)opae_calloc(1, sizeof(vfio_token));
  token->hdr.magic = VFIO_TOKEN_MAGIC;

  fpga_token t = token;
  EXPECT_EQ(FPGA_OK, vfio_fpgaDestroyToken(&t));
}

/**
 * @test    prepare_buffer_special_case
 * @brief   Test: vfio_fpgaPrepareBuffer()
 * @details When the input flags param contains<br>
 *          FPGA_BUF_PREALLOCATED, and the buf_addr and<br>
 *          len fields are both zero, then the function<br>
 *          returns FPGA_OK to signal support for pre-<br>
 *          allocated buffers.
 */
TEST(opae_v, prepare_buffer_special_case)
{
  const int flags = FPGA_BUF_PREALLOCATED;
  uint64_t wsid = 0;
  EXPECT_EQ(FPGA_OK, vfio_fpgaPrepareBuffer(nullptr, 0, nullptr, &wsid, flags));
}

/**
 * @test    prepare_buffer_err1
 * @brief   Test: vfio_fpgaPrepareBuffer()
 * @details When the input flags param contains<br>
 *          FPGA_BUF_PREALLOCATED, and the buf_addr is NULL,<br>
 *          but the len field is non-zero, then the function<br>
 *          returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, prepare_buffer_err1)
{
  const int flags = FPGA_BUF_PREALLOCATED;
  uint64_t wsid = 0;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaPrepareBuffer(nullptr, 4096, nullptr, &wsid, flags));
}

/**
 * @test    prepare_buffer_err2
 * @brief   Test: vfio_fpgaPrepareBuffer()
 * @details When the input flags param contains<br>
 *          FPGA_BUF_PREALLOCATED, and the buf_addr<br>
 *          and len fields are non-zero,<br>
 *          but the handle's vfio_pair is invalid,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, prepare_buffer_err2)
{
  const int flags = FPGA_BUF_PREALLOCATED;
  uint64_t wsid = 0;
  void *buf_addr = nullptr;
  uint64_t len = 1024 * 1024 * 1024;

  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  handle.vfio_pair = &pair;

  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaPrepareBuffer(&handle, len, &buf_addr, &wsid, flags));

  len = 2 * 1024 * 1024;
  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaPrepareBuffer(&handle, len, &buf_addr, &wsid, flags));

  len = 4096;
  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaPrepareBuffer(&handle, len, &buf_addr, &wsid, flags));
}

/**
 * @test    release_buffer_err0
 * @brief   Test: vfio_fpgaReleaseBuffer()
 * @details When the parameters are valid,<br>
 *          but the "device" member of the vfio_pair<br>
 *          is NULL,<br>
 *          then the function returns FPGA_NOT_FOUND.
 */
TEST(opae_v, release_buffer_err0)
{
  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  handle.vfio_pair = &pair;

  struct opae_vfio_buffer binfo;
  memset(&binfo, 0, sizeof(binfo));

  EXPECT_EQ(FPGA_NOT_FOUND, vfio_fpgaReleaseBuffer(&handle, (uint64_t)&binfo));
}

/**
 * @test    get_io_addr_ok
 * @brief   Test: vfio_fpgaGetIOAddress()
 * @details When the parameters are valid,<br>
 *          but the "device" member of the vfio_pair<br>
 *          is NULL,<br>
 *          then the function returns FPGA_NOT_FOUND.
 */
TEST(opae_v, get_io_addr_ok)
{
  struct opae_vfio_buffer binfo;
  memset(&binfo, 0, sizeof(binfo));
  binfo.buffer_iova = 0xdeadbeefdecafbad;

  uint64_t ioaddr = 0;

  EXPECT_EQ(FPGA_OK, vfio_fpgaGetIOAddress(nullptr, (uint64_t)&binfo, &ioaddr));
  EXPECT_EQ(0xdeadbeefdecafbad, ioaddr);
}


/**
 * @test    create_event_err0
 * @brief   Test: vfio_fpgaCreateEventHandle()
 * @details When the input event handle pointer<br>
 *          is NULL, then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, create_event_err0)
{
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaCreateEventHandle(nullptr));
}

/**
 * @test    create_event_err1
 * @brief   Test: vfio_fpgaCreateEventHandle()
 * @details When malloc fails,<br>
 *          then the function returns FPGA_NO_MEMORY.
 */
TEST(opae_v, create_event_err1)
{
#ifndef OPAE_ENABLE_MOCK
  GTEST_SKIP() << "Invalidate test requires MOCK.";
#endif // OPAE_ENABLE_MOCK
      
  fpga_event_handle eh = nullptr;

  test_system::instance()->invalidate_malloc(0, "vfio_fpgaCreateEventHandle");

  EXPECT_EQ(FPGA_NO_MEMORY, vfio_fpgaCreateEventHandle(&eh));
}

/**
 * @test    create_event_ok
 * @brief   Test: vfio_fpgaCreateEventHandle()
 * @details When the function is able to create,<br>
 *          an event handle, then it places its<br>
 *          address in the output parameter and<br>
 *          returns FPGA_OK.
 */
TEST(opae_v, create_event_ok)
{
  fpga_event_handle eh = nullptr;

  EXPECT_EQ(FPGA_OK, vfio_fpgaCreateEventHandle(&eh));

  vfio_event_handle *_veh = (vfio_event_handle *)eh;
  ASSERT_NE(nullptr, _veh);

  EXPECT_EQ(VFIO_EVENT_HANDLE_MAGIC, _veh->magic);
  EXPECT_GE(_veh->fd, 0);
  close(_veh->fd);
  EXPECT_EQ(0, _veh->flags);

  EXPECT_EQ(0, pthread_mutex_destroy(&_veh->lock));
  opae_free(_veh);
}

/**
 * @test    destroy_event_err0
 * @brief   Test: vfio_fpgaDestroyEventHandle()
 * @details When the given event handle pointer is NULL,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, destroy_event_err0)
{
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyEventHandle(nullptr));
}

/**
 * @test    destroy_event_err1
 * @brief   Test: vfio_fpgaDestroyEventHandle()
 * @details When the given event handle pointer<br>
 *          has an invalid magic field,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, destroy_event_err1)
{
  vfio_event_handle ueh;
  memset(&ueh, 0, sizeof(ueh));

  fpga_event_handle eh = &ueh;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyEventHandle(&eh));
}

/**
 * @test    destroy_event_err2
 * @brief   Test: vfio_fpgaDestroyEventHandle()
 * @details When the given event handle pointer<br>
 *          has an invalid file descriptor,<br>
 *          then the function returns FPGA_INVALID_PARAM.
 */
TEST(opae_v, destroy_event_err2)
{
  vfio_event_handle veh;
  memset(&veh, 0, sizeof(veh));
  veh.fd = -1;
  veh.magic = VFIO_EVENT_HANDLE_MAGIC;

  fpga_event_handle eh = &veh;
  EXPECT_EQ(FPGA_INVALID_PARAM, vfio_fpgaDestroyEventHandle(&eh));
}

/**
 * @test    destroy_event_ok
 * @brief   Test: vfio_fpgaDestroyEventHandle()
 * @details When the given event handle pointer<br>
 *          is valid,<br>
 *          then the function returns FPGA_OK.
 */
TEST(opae_v, destroy_event_ok)
{
  vfio_event_handle *ueh = (vfio_event_handle *)opae_calloc(1, sizeof(*ueh));
  ASSERT_NE(nullptr, ueh);

  ueh->magic = VFIO_EVENT_HANDLE_MAGIC;
  ueh->lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  ueh->fd = open("/dev/null", O_RDONLY);

  fpga_event_handle eh = ueh;
  EXPECT_EQ(0, vfio_fpgaDestroyEventHandle(&eh));
}

/**
 * @test    get_os_object
 * @brief   Test: vfio_fpgaGetOSObjectFromEventHandle()
 * @details When the given event handle pointer<br>
 *          is valid,<br>
 *          then the function retrieves the fd,<br>
 *          and returns FPGA_OK.
 */
TEST(opae_v, get_os_object)
{
  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.fd = 3;
  eh.flags = 0;

  int fd = 0;
  EXPECT_EQ(FPGA_OK, vfio_fpgaGetOSObjectFromEventHandle(&eh, &fd));
  EXPECT_EQ(3, fd);
}

/**
 * @test    register_event_ok
 * @brief   Test: register_event()
 * @details When the given event type is<br>
 *          FPGA_EVENT_INTERRUPT,<br>
 *          but the "device" pointer of the vfio_pair_t<br>
 *          is NULL,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, register_event_ok)
{
  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.vfio_pair = &pair;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.flags = 0; 

  EXPECT_EQ(FPGA_EXCEPTION, register_event(&handle, FPGA_EVENT_INTERRUPT, &eh, 7));
  EXPECT_EQ(7, eh.flags);
}

/**
 * @test    register_event_err0
 * @brief   Test: register_event()
 * @details When the given event type is not<br>
 *          FPGA_EVENT_INTERRUPT,<br>
 *          then the function returns FPGA_NOT_SUPPORTED.
 */
TEST(opae_v, register_event_err0)
{
  vfio_handle handle;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.flags = 0; 

  EXPECT_EQ(FPGA_NOT_SUPPORTED, register_event(&handle, FPGA_EVENT_ERROR, &eh, 0));
  EXPECT_EQ(FPGA_NOT_SUPPORTED, register_event(&handle, FPGA_EVENT_POWER_THERMAL, &eh, 0));
}

/**
 * @test    register_event_err1
 * @brief   Test: register_event()
 * @details When the given event type is out of bounds,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, register_event_err1)
{
  vfio_handle handle;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.flags = 0; 

  EXPECT_EQ(FPGA_EXCEPTION, register_event(&handle, (fpga_event_type)99, &eh, 0));
}

/**
 * @test    RegisterEvent_err0
 * @brief   Test: vfio_fpgaRegisterEvent()
 * @details When the given event handle is invalid,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, RegisterEvent_err0)
{
  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  vfio_event_handle eh;
  eh.magic = ~VFIO_EVENT_HANDLE_MAGIC; // <- make eh invalid
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.fd = 3;
  eh.flags = 0; 

  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaRegisterEvent(&handle, FPGA_EVENT_INTERRUPT, &eh, 7));
}

/**
 * @test    RegisterEvent_ok
 * @brief   Test: vfio_fpgaRegisterEvent()
 * @details When the parameters are valid,<br>
 *          but the "device" pointer in the vfio_pair_t is NULL,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, RegisterEvent_ok)
{
  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  handle.vfio_pair = &pair;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.fd = 3;
  eh.flags = 0; 

  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaRegisterEvent(&handle, FPGA_EVENT_INTERRUPT, &eh, 7));
  EXPECT_EQ(3, eh.fd);
  EXPECT_EQ(7, eh.flags);
}

/**
 * @test    unregister_event_ok
 * @brief   Test: unregister_event()
 * @details When the given event type is<br>
 *          FPGA_EVENT_INTERRUPT,<br>
 *          then the function returns FPGA_OK.
 */
TEST(opae_v, unregister_event_ok)
{
  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.vfio_pair = &pair;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  eh.flags = 0;

  EXPECT_EQ(FPGA_EXCEPTION, unregister_event(&handle, FPGA_EVENT_INTERRUPT, &eh));
}

/**
 * @test    unregister_event_err0
 * @brief   Test: unregister_event()
 * @details When the given event type is not<br>
 *          FPGA_EVENT_INTERRUPT,<br>
 *          then the function returns FPGA_NOT_SUPPORTED.
 */
TEST(opae_v, unregister_event_err0)
{
  vfio_handle handle;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  EXPECT_EQ(FPGA_NOT_SUPPORTED, unregister_event(&handle, FPGA_EVENT_ERROR, &eh));
  EXPECT_EQ(FPGA_NOT_SUPPORTED, unregister_event(&handle, FPGA_EVENT_POWER_THERMAL, &eh));
}

/**
 * @test    unregister_event_err1
 * @brief   Test: unregister_event()
 * @details When the given event type is out of bounds,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, unregister_event_err1)
{
  vfio_handle handle;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  EXPECT_EQ(FPGA_EXCEPTION, unregister_event(&handle, (fpga_event_type)99, &eh));
}

/**
 * @test    UnregisterEvent_err0
 * @brief   Test: vfio_fpgaUnregisterEvent()
 * @details When the given event handle is invalid,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, UnregisterEvent_err0)
{
  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  vfio_event_handle eh;
  eh.magic = ~VFIO_EVENT_HANDLE_MAGIC; // <- make eh invalid
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaUnregisterEvent(&handle, FPGA_EVENT_INTERRUPT, &eh));
}

/**
 * @test    UnregisterEvent_ok
 * @brief   Test: vfio_fpgaUnregisterEvent()
 * @details When the parameters are valid,<br>
 *          but the "device" pointer in the vfio_pair_t is NULL,<br>
 *          then the function returns FPGA_EXCEPTION.
 */
TEST(opae_v, UnregisterEvent_ok)
{
  vfio_pair_t pair;
  memset(&pair, 0, sizeof(pair));

  vfio_handle handle;
  memset(&handle, 0, sizeof(handle));
  handle.magic = VFIO_HANDLE_MAGIC;
  handle.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  handle.vfio_pair = &pair;

  vfio_event_handle eh;
  eh.magic = VFIO_EVENT_HANDLE_MAGIC;
  eh.lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

  EXPECT_EQ(FPGA_EXCEPTION, vfio_fpgaUnregisterEvent(&handle, FPGA_EVENT_INTERRUPT, &eh));
}
