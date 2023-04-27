#include <stddef.h>

void test_mock_server_init(void (*on_output)(const char *data));
void test_mock_server_deinit();
void test_mock_server_read(const char *data);
