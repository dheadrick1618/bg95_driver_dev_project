idf_component_register(
	SRCS
	"test_main.c"
	"test_dummy.c"
	"test_at_cmd_parser.c"
	"test_mock_uart_interface.c"
	"test_at_cmd_handler.c"
	#### INDIVIDUAL AT CMD TESTING NOW #### 
	"test_at_cmd_cpin.c"
	"test_at_cmd_csq.c"
	"test_at_cmd_cops.c"
	INCLUDE_DIRS
	"."
	REQUIRES
	unity
	freertos
	bg95_driver
)

