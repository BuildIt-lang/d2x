#include "xray/xray.h"
#include <utility>
namespace xray {

/* XRay generates the following objects for each function info that it emits

1. The very first object we have is a source_table which is a array of source_stack of size number of lines
in the function

struct xray_source_stack {
	int stack_size;
	int stack_offset;
}

2. The second object is the source_list which is an array of source_loc. The stack_offset in source_stack
in an offset into this array. The size is as much as in required by the source_stack

struct xray_source_loc {
	int filename;
	int linenumber;
}

3. The third object is a string list which is just an array of char*. All string variables are just offsets into 
this table. 

4. After this we have a function_header which just has back pointers and sizes for above arrays. This also has information
about the function itself.

struct xray_function_header {
	void (*)(void) function_addr; // start address of the function for matching

	int source_table_len; // Equal to number of lines in the function
	struct xray_source_stack* source_table; // points to 1

	int source_list_len;
	struct xray_source_loc* source_list; // points to 2

	int string_table_len;
	char** string_table; // points to 3
}

5. After this finally there is a single pointer to the function_header that is placed in a special XRAY_entry section 
*/


int xray_context::get_string_id(std::string s) {
	if (reverse_string_table.find(s) != reverse_string_table.end()) {
		return reverse_string_table[s];
	}
	int idx = string_table.size();
	string_table.push_back(s);
	reverse_string_table[s] = idx;
	return idx;
}


void xray_context::emit_function_info(std::ostream &oss) {
	oss << "/*  Begin debug information for function: " << current_function_name << " */\n";		
	string_table.clear();
	source_list.clear();	
	source_table.clear();
	
	for (int lno = 0; lno < (int)source_loc_table.size(); lno++) {
		auto &frames = source_loc_table[lno];
		source_table.push_back(std::make_pair(frames.size(), source_list.size()));
		for (auto frame: frames) {
			std::string name = frame.file;
			int line = frame.line;
			int name_id = get_string_id(name);
			source_list.push_back(std::make_pair(name_id, line));	
		}		
	}

	// Emit 1
	oss << "struct xray_source_stack xray_" << current_function_name << "_source_table[] = {\n";
	for (auto v: source_table) {
		oss << ident_char << "{" << v.first << ", " << v.second << "},\n";
	}
	oss << "};\n";

	// Emit 2
	oss << "struct xray_source_loc xray_" << current_function_name << "_source_list[] = {\n";
	for (auto v: source_list) {
		oss << ident_char << "{" << v.first << ", " << v.second << "},\n";
	}
	oss << "};\n";
	
	// Emit 3
	oss << "char* xray_" << current_function_name << "_string_table[] = {\n";
	for (auto v: string_table) {
		oss << ident_char << "(char*)\"" << v << "\",\n";
	}
	oss << "};\n";

	// Emit 4		
	oss << "struct xray_function_header xray_" << current_function_name << "_function_header = {\n";
	// TODO: Change this to take/compute a separate function address expression
	// For now we assume all functions are C style functions and the address expression is simply the name
	oss << ident_char << "(unsigned long long)" << current_function_name << ", \n";
	oss << ident_char << (int)source_table.size() << ", \n";
	oss << ident_char << "xray_" << current_function_name << "_source_table" << ", \n";
	oss << ident_char << (int)source_list.size() << ", \n";
	oss << ident_char << "xray_" << current_function_name << "_source_list" << ", \n";
	oss << ident_char << (int)string_table.size() << ", \n";
	oss << ident_char << "xray_" << current_function_name << "_string_table" << ", \n";
	oss << "};\n";

	// Emit 5
	oss << "struct xray_function_header* xray_" << current_function_name << "_function_header_entry __attribute__"
		"((section(\"" << debug_entry_section << "\"))) = &xray_" << current_function_name << "_function_header" 
		";\n";


	oss << "/*  End debug information for function: " << current_function_name << " */\n";		
}

}
