#ifndef XRAY_RUNTIME_H
#define XRAY_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

struct xray_source_stack {
	int stack_size;
	int stack_offset;
};
struct xray_source_loc {
	int filename;
	int linenumber;
};

struct xray_function_header {
	unsigned long long function_addr; // start address of the function for matching

	int source_table_len; // Equal to number of lines in the function
	struct xray_source_stack* source_table; // points to 1

	int source_list_len;
	struct xray_source_loc* source_list; // points to 2

	int string_table_len;
	char** string_table; // points to 3
};





#ifdef __cplusplus
}
#endif

#endif
