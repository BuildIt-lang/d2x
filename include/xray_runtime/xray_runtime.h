#ifndef XRAY_RUNTIME_H
#define XRAY_RUNTIME_H
#include <iostream>
#include <vector>
#include <stdint.h>
#include <dlfcn.h>
#include <cstring>
#include <link.h>

namespace xray {
namespace runtime {

struct xray_source_stack {
	int stack_size;
	int stack_offset;
};
struct xray_source_loc {
	int filename;
	int linenumber;
	int function;
	int foffset;
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

extern std::vector<xray_function_header*> *registered_function_headers;
static void xray_headers_init(void) {
	if (registered_function_headers == nullptr) {
		registered_function_headers = new std::vector<xray_function_header*>();
	}
}

struct xray_register_header {
	xray_register_header(struct xray_function_header *h) {
		xray_headers_init();
		registered_function_headers->push_back(h);
	}	
};

struct xray_context {
	// Executable info	
	const char* dli_fname;
	uint64_t load_offset;

	uint64_t function;
	xray_function_header* header;	

	int address_line;
	int function_line;
};


struct xray_context find_context(uint64_t ip, uint64_t sp);
std::string get_backtrace(struct xray_context ctx);
std::string get_listing(struct xray_context ctx);
std::string get_frame(struct xray_context ctx, const char*);

/* API functions to be invoked from the debugger */
namespace cmd {
void xbt(uint64_t ip, uint64_t sp);
void xlist(uint64_t ip, uint64_t sp);
void xframe(uint64_t ip, uint64_t sp, const char*);
}
/* End of API functions */



}
}


#endif
