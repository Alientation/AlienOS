#include "unit_tests.h"
#include "kmalloc.h"
#include "kernel.h"
#include "io.h"

static const char *test_alloc (void)
{
    kmalloc_printdebug ();

    struct KMStats stats = kmalloc_getstats ();

    void * const ptr1 = kmalloc (0);
    if (!ptr1) return "Failed kmalloc(0)";
    kfree (ptr1);

    io_serial_printf (COMPort_1, "1\n");
    kmalloc_printdebug ();

    uint8_t * const ptr2 = (uint8_t *) kmalloc (1);
    const uint8_t ptr2_data = 0x7A;
    if (!ptr2) return "Failed kmalloc(1)";
    ptr2[0] = ptr2_data;

    io_serial_printf (COMPort_1, "2\n");
    kmalloc_printdebug ();

    uint32_t * const ptr3 = (uint32_t *) kmalloc (16);
    const uint32_t ptr3_data[4] = {0xFEEDF00D, 0xF00BAD12, 0x1357ACEF, 0x02468BDE};
    if (!ptr3) return "Failed kmalloc(16)";
    for (size_t i = 0; i < sizeof (ptr3_data) / sizeof (ptr3_data[0]); i++) ptr3[i] = ptr3_data[i];
    if (ptr2[0] != ptr2_data) return "Corrupted kmalloc(1)'s memory";
    kfree (ptr2);

    io_serial_printf (COMPort_1, "3\n");
    kmalloc_printdebug ();

    uint32_t * const ptr4 = (uint32_t *) kmalloc (4097 * 4);
    if (!ptr4) return "Failed kmalloc(4097 * 4)";
    for (size_t i = 0; i < 4097; i++) ptr4[i] = i;
    for (size_t i = 0; i < sizeof (ptr3_data) / sizeof (ptr3_data[0]); i++)
        if (ptr3[i] != ptr3_data[i]) return "Corrupted kmalloc(16)'s memory";
    kfree (ptr3);

    kmalloc_printdebug ();

    kfree (ptr4);

    io_serial_printf (COMPort_1, "4\n");
    kmalloc_printdebug ();

    struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes) return "Memory Leak";

    io_serial_printf (COMPort_1, "Passed test_alloc()\n");
    kmalloc_printdebug ();
    return NULL;
}


void kmalloc_test (struct UnitTestsResult * const result)
{
    const char * res = test_alloc ();
    result->total_tests++;
    if (res) {
        io_serial_printf (COMPort_1, "%s\n", res);
        result->failed_tests++;
    }
}
