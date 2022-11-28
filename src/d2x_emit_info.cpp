#include "d2x/d2x.h"
#include <utility>
#include "blocks/c_code_generator.h"
namespace d2x {

/* D2X generates the following objects for each function info that it emits

1.a The very first object we have is a source_table which is an array of source_stack of size number of lines
in the function

struct d2x_source_stack {
	int stack_size;
	int stack_offset;
}

1.b The second object is the source_list which is an array of source_loc. The stack_offset in source_stack
in an offset into this array. The size is as much as is required by the source_stack

struct d2x_source_loc {
	int filename;
	int linenumber;
	int functionname;
	int foffset;
}

2.a The next object is var_table which is an array of var_stack of size number of lines in the function

struct d2x_var_stack {
	int stack_size;
	int stack_offset;
}

2.b The next object is the var_list which is an array of var_entry. The stack_offset in var_stack is an offset into
this array. The size is as much as is required by the var_stack

struct d2x_var_entry {
	int varname;
	int varvalue;
	unsigned long long rvarvalue;
}

3. The third object is a string list which is just an array of char*. All string variables are just offsets into 
this table. 

4. After this we have a function_header which just has back pointers and sizes for above arrays. This also has information
about the function itself.

struct d2x_function_header {
	void (*)(void) function_addr; // start address of the function for matching

	int source_table_len; // Equal to number of lines in the function
	struct d2x_source_stack* source_table; // points to 1.a

	int source_list_len;
	struct d2x_source_loc* source_list; // points to 1.b

	int var_table_len; // Equal to number of lines in the function
	struct d2x_var_stack* var_table; // points to 2.a
	
	int var_list_len;
	struct d2x_var_entry* var_list // points to 2.b

	int string_table_len;
	char** string_table; // points to 3

	
	// scratch space for use at runtime
	const char* identified_filename;
	int identified_line;
}

5. Finally there is a constructor call to d2x_register_header
*/


int d2x_context::get_string_id(std::string s) {
	if (reverse_string_table.find(s) != reverse_string_table.end()) {
		return reverse_string_table[s];
	}
	int idx = string_table.size();
	string_table.push_back(s);
	reverse_string_table[s] = idx;
	return idx;
}


void d2x_context::emit_function_info(std::ostream &oss) {
	oss << "/*  Begin debug information for section: " << current_anchor_counter << " */\n";		
	string_table.clear();
	reverse_string_table.clear();
	emit_source_list.clear();	
	emit_source_table.clear();
	emit_var_table.clear();
	emit_var_list.clear();
	used_resolvers.clear();
	
	for (int lno = 0; lno < (int)source_loc_table.size(); lno++) {
		auto &frames = source_loc_table[lno];
		emit_source_table.push_back(std::make_pair(frames.size(), emit_source_list.size()));
		for (auto frame: frames) {
			std::string name = frame.file;
			int line = frame.line;
			int name_id = get_string_id(name);
			int fname_id = get_string_id(frame.fname);
			int fline = frame.foffset;
			emit_source_list.push_back(std::make_tuple(name_id, line, fname_id, fline));	
		}		
	}

	for (int lno = 0; lno < (int) var_table.size(); lno++) {
		auto &vars = var_table[lno];
		emit_var_table.push_back(std::make_pair(vars.size(), emit_var_list.size()));
		for (auto const& var: vars) {
			int varname = get_string_id(var.first);
			int varvalue = get_string_id(var.second.value);
			runtime_value_resolver* rvarvalue = var.second.rvalue;
			if (rvarvalue != nullptr) {
				if (std::find(used_resolvers.begin(), used_resolvers.end(), rvarvalue) 
					== used_resolvers.end()) {
					if (std::find(emitted_resolvers.begin(), emitted_resolvers.end(), rvarvalue) 
						== emitted_resolvers.end()) {	
						used_resolvers.push_back(rvarvalue);
					}
				}
			}
			emit_var_list.push_back(std::make_pair(varname, std::make_pair(varvalue, rvarvalue)));
		}
	}

	// Before we emit any datastructures, we should emit the resolvers
	for (auto r: used_resolvers) {
		r->gen_resolver();
		block::c_code_generator generator(oss);
		generator.curr_indent = 0;
		generator.use_d2x = false;
		r->resolver->accept(&generator);
		oss << std::endl;	
		emitted_resolvers.push_back(r);
	}


	// Emit 1.a
	oss << "static struct d2x::runtime::d2x_source_stack d2x_" << current_anchor_counter << "_source_table[] = {\n";
	int index = 0;
	for (auto v: emit_source_table) {
		oss << ident_char << "{" << v.first << ", " << v.second << "}, //" << index << "\n";
		index++;
	}
	oss << "};\n";

	// Emit 1.b
	index = 0;
	oss << "static struct d2x::runtime::d2x_source_loc d2x_" << current_anchor_counter << "_source_list[] = {\n";
	for (auto v: emit_source_list) {
		oss << ident_char << "{" << std::get<0>(v) << ", " << std::get<1>(v) << ", " << std::get<2>(v) << ", " << std::get<3>(v) << "}, //" << index << "\n";
		index++;
	}
	oss << "};\n";

	// Emit 2.a
	oss << "static struct d2x::runtime::d2x_var_stack d2x_" << current_anchor_counter << "_var_table[] = {\n";
	for (auto v: emit_var_table) {
		oss << ident_char << "{" << v.first << ", " << v.second << "},\n";
	}
	oss << "};\n";

	// Emit 2.b
	oss << "static struct d2x::runtime::d2x_var_entry d2x_" << current_anchor_counter << "_var_list[] = {\n";
	for (auto v: emit_var_list) {
		if (v.second.second == nullptr)
			oss << ident_char << "{" << v.first << ", " << v.second.first << ", 0},\n";
		else 
			oss << ident_char << "{" << v.first << ", -1, (unsigned long long)" << v.second.second->resolver_name << "},\n";
	}
	oss << "};\n";
	
	// Emit 3
	oss << "static const char* d2x_" << current_anchor_counter << "_string_table[] = {\n";
	for (auto v: string_table) {
		oss << ident_char << "\"" << v << "\",\n";
	}
	oss << "};\n";

	// Emit 4		
	oss << "static struct d2x::runtime::d2x_function_header d2x_" << current_anchor_counter << "_function_header = {\n";
	// TODO: Change this to take/compute a separate function address expression
	// For now we assume all functions are C style functions and the address expression is simply the name
	oss << ident_char << "(unsigned long long)" << current_anchor_name << ", \n";
	oss << ident_char << (int)emit_source_table.size() << ", \n";
	oss << ident_char << "d2x_" << current_anchor_counter << "_source_table" << ", \n";
	oss << ident_char << (int)emit_source_list.size() << ", \n";
	oss << ident_char << "d2x_" << current_anchor_counter << "_source_list" << ", \n";

	oss << ident_char << (int)emit_var_table.size() << ", \n";
	oss << ident_char << "d2x_" << current_anchor_counter << "_var_table" << ", \n";
	oss << ident_char << (int)emit_var_list.size() << ", \n";
	oss << ident_char << "d2x_" << current_anchor_counter << "_var_list" << ", \n";

	oss << ident_char << (int)string_table.size() << ", \n";
	oss << ident_char << "d2x_" << current_anchor_counter << "_string_table" << ", \n";

	// Scratch values initialized to NULL and -1 
	oss << ident_char << "NULL, \n";
	oss << ident_char << "-1, \n";

	oss << "};\n";

	// Emit 5
	
	oss << "static struct d2x::runtime::d2x_register_header d2x_" << current_anchor_counter << "_function_header_entry"
		" (&d2x_" << current_anchor_counter << "_function_header);\n";


	oss << "/*  End debug information for section: " << current_anchor_counter << " */\n";		
}

}
