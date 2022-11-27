#ifndef D2X_RUNTIME_H
#define D2X_RUNTIME_H
#include <iostream>
#include <vector>
#include <stdint.h>
#include <dlfcn.h>
#include <cstring>
#include <link.h>
#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>

namespace d2x {
namespace runtime {

struct d2x_source_stack {
	int stack_size;
	int stack_offset;
};
struct d2x_source_loc {
	int filename;
	int linenumber;
	int function;
	int foffset;
};

struct d2x_var_stack {
	int stack_size;
	int stack_offset;
};

struct d2x_var_entry {
	int varname;
	int varvalue;
	unsigned long long rvarvalue;
};

struct d2x_function_header {
	unsigned long long function_addr; // start address of the function for matching

	int source_table_len; // Equal to number of lines in the function
	struct d2x_source_stack* source_table; // points to 1.a

	int source_list_len;
	struct d2x_source_loc* source_list; // points to 1.b

	int var_table_len;
	struct d2x_var_stack* var_table; // points to 2.a
	
	int var_list_len;
	struct d2x_var_entry* var_list; // points to 2.b

	int string_table_len;
	const char** string_table; // points to 3

	/* scratch space for use at runtime */
	const char* identified_filename;
	int identified_line;
};

extern std::vector<d2x_function_header*> *registered_function_headers;
static void d2x_headers_init(void) {
	if (registered_function_headers == nullptr) {
		registered_function_headers = new std::vector<d2x_function_header*>();
	}
}

struct d2x_register_header {
	d2x_register_header(struct d2x_function_header *h) {
		d2x_headers_init();
		registered_function_headers->push_back(h);
	}	
};

struct d2x_context {
	// Register info
	uint64_t rip;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rbx;

	// Executable info	
	const char* dli_fname;
	uint64_t load_offset;

	uint64_t function;
	d2x_function_header* header;	

	int address_line;
	int function_line;

	const char* src_filename;
	
	// Debug info
	Dwarf_Debug dbg;
	Dwarf_Die cu;
};


struct d2x_context find_context(void* ip, void* sp, void* bp, void* bx);
std::string get_backtrace(struct d2x_context ctx);
std::string get_listing(struct d2x_context ctx);
std::string get_frame(struct d2x_context ctx, const char*);
std::string get_vars(struct d2x_context ctx, const char*);

namespace rtv {
	void* find_stack_var(std::string varname);
}


/* API functions to be invoked from the debugger */
namespace cmd {
void xbt(void* ip, void* sp, void* bp, void* bx);
void xlist(void* ip, void* sp, void* bp, void* bx);
void xframe(void* ip, void* sp, void* bp, void* bx, const char*);
void xvars(void* ip, void* sp, void* bp, void* bx, const char*);
void xfvl(void* ip, void* sp, void* bp, void* bx, const char*);
}
/* End of API functions */



}
}


#endif
