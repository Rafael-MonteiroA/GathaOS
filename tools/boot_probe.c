/* Linked only into the large-kernel boot test, never into a release image. */
#include "boot.h"
#include "drivers/serial.h"
#include "arch/x86/io.h"
#include <stdint.h>
extern const volatile uint32_t boot_probe_data[6 * 1024 * 1024 / 4];
extern volatile uint32_t boot_probe_bss[5 * 1024 * 1024 / 4];
extern void normal_kernel_main(const struct boot_info *);
static void fail(void) {
    klog("BOOTTEST: large data or BSS FAIL\r\n");
    for (;;) __asm__ volatile("cli; hlt");
}
void kernel_main(const struct boot_info *boot) {
    serial_init();
    for (uint32_t i = 0; i < 6 * 1024 * 1024 / 4; ++i)
        if (boot_probe_data[i] != (i ^ UINT32_C(0x735a91c6))) fail();
    for (uint32_t i = 0; i < 5 * 1024 * 1024 / 4; ++i) {
        if (boot_probe_bss[i]) fail();
        boot_probe_bss[i] = i ^ UINT32_C(0x19f73da2);
    }
    for (uint32_t i = 0; i < 5 * 1024 * 1024 / 4; ++i)
        if (boot_probe_bss[i] != (i ^ UINT32_C(0x19f73da2))) fail();
    klog("BOOTTEST: 6 MiB data and 5 MiB BSS PASS\r\n");
    normal_kernel_main(boot);
}
