#include "alienos/tests/unit_tests.h"


// static void test_io (void)
// {
// 	io_serial_set_loopback (COMPort_1, true);
// 	io_serial_outb (COMPort_1, 0xAB);

// 	uint8_t data = 0;
// 	if (!io_serial_nextinb (COMPort_1, &data))
// 	{
// 		terminal_writestr ("FAILED SPIN ITERATIONS\n\n");
// 	}
// 	else if (data != 0xAB)
// 	{
// 		terminal_writestr ("FAILED DATA MATCH\n\n");
// 	}

// 	io_serial_set_loopback (COMPort_1, false);

// 	io_serial_outstr (COMPort_1, "abc\n");
// 	io_serial_outint (COMPort_1, 206);
// 	io_serial_outstr (COMPort_1, "\n");
// 	io_serial_outint (COMPort_1, -206);
// 	io_serial_outstr (COMPort_1, "\n");
// 	io_serial_outbool (COMPort_1, false);
// 	io_serial_outstr (COMPort_1, "\n");
// 	io_serial_outbool (COMPort_1, true);
// 	io_serial_outstr (COMPort_1, "\n");
// }

void io_test (struct UnitTestsResult * const result)
{

}
