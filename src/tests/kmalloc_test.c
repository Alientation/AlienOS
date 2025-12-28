#include "unit_tests.h"
#include "kmalloc.h"
#include "kernel.h"
#include "io.h"

#include <string.h>

static const char *test_alloc (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_alloc()\n");
    const struct KMStats stats = kmalloc_getstats ();

    void * const ptr1 = kmalloc (0);
    if (!ptr1) return "Failed: kmalloc(0)";
    kfree (ptr1);

    uint8_t * const ptr2 = (uint8_t *) kmalloc (1);
    const uint8_t ptr2_data = 0x7A;
    if (!ptr2) return "Failed: kmalloc(1)";
    ptr2[0] = ptr2_data;

    uint32_t * const ptr3 = (uint32_t *) kmalloc (16);
    const uint32_t ptr3_data[4] = {0xFEEDF00D, 0xF00BAD12, 0x1357ACEF, 0x02468BDE};
    if (!ptr3) return "Failed: kmalloc(16)";
    for (size_t i = 0; i < sizeof (ptr3_data) / sizeof (ptr3_data[0]); i++) ptr3[i] = ptr3_data[i];
    if (ptr2[0] != ptr2_data) return "Failed: corrupted kmalloc(1)'s memory";
    kfree (ptr2);

    uint32_t * const ptr4 = (uint32_t *) kmalloc (4097 * 4);
    if (!ptr4) return "Failed: kmalloc(4097 * 4)";
    for (size_t i = 0; i < 4097; i++) ptr4[i] = i;
    for (size_t i = 0; i < sizeof (ptr3_data) / sizeof (ptr3_data[0]); i++)
        if (ptr3[i] != ptr3_data[i])
        {
            io_serial_printf (COMPort_1, "%x,%x\n", ptr3[i], ptr3_data[i]);
            return "Failed: corrupted kmalloc(16)'s memory";
        }
    kfree (ptr3);
    kfree (ptr4);

    const struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes)
        return "Failed: memory leak";

    io_serial_printf (COMPort_1, "Passed test_alloc()\n");
    return NULL;
}

static const char *test_calloc (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_calloc()\n");

    uint32_t * const p1 = kmalloc (16);
    const uint32_t p1_data[4] = {0xFEEDF00D, 0xF00BAD12, 0x1357ACEF, 0x02468BDE};
    for (size_t i = 0; i < sizeof (p1_data) / sizeof (p1_data[0]); i++) p1[i] = p1_data[i];

    kfree (p1);

    uint32_t * const p2 = kcalloc (4, 4);
    if ((uintptr_t) p1 != (uintptr_t) p2) return "Failed: insertion";
    for (size_t i = 0; i < 4; i++) if (p2[i] != 0) return "Failed: did not clear memory";

    kfree (p2);

    io_serial_printf (COMPort_1, "Passed test_calloc()\n");
    return NULL;
}

static const char *test_realloc (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_realloc()\n");

    void * const p1 = kmalloc (4);
    if (!p1) return "Failed: kmalloc(4)";

    void *const p2 = krealloc (p1, 8);
    if ((uintptr_t) p1 != (uintptr_t) p2) return "Failed: resized when original block is large enough";

    uint32_t * const p3 = krealloc (p2, 32);
    if ((uintptr_t) p2 != (uintptr_t) p3) return "Failed: did not resize block";

    for (size_t i = 0; i < 8; i++) p3[i] = i;

    void * const p4 = kmalloc (32);
    uint32_t * const p5 = krealloc (p3, 64);
    if ((uintptr_t) p5 == (uintptr_t) p3) return "Failed: did not reallocate block";

    for (size_t i = 0; i < 8; i++)
        if (p5[i] != i) return "Failed: overlapping memory blocks";

    kfree (p4);
    kfree (p5);

    io_serial_printf (COMPort_1, "Passed test_realloc()\n");
    return NULL;
}

static const char *test_short (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_alloc()\n");
    const struct KMStats stats = kmalloc_getstats ();

    uint32_t *ps[32];
    for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
    {
        ps[i] = kmalloc (16);
        if (!ps[i]) return "Failed: kmalloc(16)";
        for (size_t j = 0; j < 4; j++)
        {
            ps[i][j] = i * 4 + j;
        }
    }
    uint32_t * const large = kmalloc (16 * 4096);
    if (!large) return "Failed: kmalloc(16 * 4096)";
    memset (large, 0xF0, 16 * 4096);
    kfree (large);

    for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
    {
        for (size_t j = 0; j < 4; j++)
        {
            if (ps[i][j] != i * 4 + j) return "Failed: overlapping memory blocks";
        }
    }

    for (size_t k = 1; k < 256; k++)
    {
        for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
        {
            for (size_t j = 0; j < 4; j++)
            {
                if (ps[i][j] != (k - 1) * 128 + i * 4 + j) return "Failed: overlapping memory blocks";
            }

            kfree (ps[i]);
            ps[i] = kmalloc (16);
            if (!ps[i]) return "Failed: kmalloc(16)";
            for (size_t j = 0; j < 4; j++)
            {
                ps[i][j] = k * 128 + i * 4 + j;
            }
        }
    }

    for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
    {
        kfree (ps[i]);
    }

    kmalloc_printdebug ();

    const struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes)
        return "Failed: memory leak";
    return NULL;
}

static const char *test_extensive (void)
{
    /* TODO: */
    return NULL;
}

static const char *test_free (void)
{
    io_serial_printf (COMPort_1, "\nRunning test_free()\n");
    const struct KMStats stats = kmalloc_getstats ();

    kfree (NULL);

    void * const p1 = kmalloc (16);
    void * const p2 = kmalloc (16);
    void * const p3 = kmalloc (16);
    void * const p4 = kmalloc (16);

    kfree (p2);
    void * const p5 = kmalloc (16);
    if ((uintptr_t) p2 != (uintptr_t) p5) return "Failed: insertion";

    kfree (p5);
    kfree (p3);
    kfree (p4);
    void * const p6 = kmalloc (48);
    if ((uintptr_t) p6 != (uintptr_t) p2) return "Failed: coalescing";

    kfree (p1);
    kfree (p6);

    const struct KMStats stats_now = kmalloc_getstats ();
    if (stats.allocation_bytes - stats.free_bytes != stats_now.allocation_bytes - stats_now.free_bytes)
        return "Failed: memory leak";

    io_serial_printf (COMPort_1, "Passed test_free()\n");
    return NULL;
}

void kmalloc_test (struct UnitTestsResult * const result)
{
    run_test (test_alloc, result);
    run_test (test_calloc, result);
    run_test (test_realloc, result);
    run_test (test_free, result);

    kmalloc_disabledebug ();
    run_test (test_short, result);
    run_test (test_extensive, result);
    kmalloc_enabledebug ();
}
