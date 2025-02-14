idf_component_register(
	SRCS
	"test_main.c"
	"test_dummy.c"
	"test_at_cmd_parser.c"
	INCLUDE_DIRS
	"."
	REQUIRES
	unity
	freertos
	bg95_driver
)

