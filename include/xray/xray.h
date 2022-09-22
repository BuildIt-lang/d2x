#ifndef XRAY_H
#define XRAY_H
#include <string>
#include <vector>
#include <map>
#include <ostream>

#include "blocks/block.h"
#include "builder/dyn_var.h"
#include "builder/builder_context.h"
#include "blocks/rce.h"
#include <functional>

namespace xray {

struct source_loc {
	std::string file;
	int line;

	// Set this to empty if not in use
	std::string fname;
	int foffset;
};

namespace rt {
extern const char string_t_name[];
using string = builder::name<string_t_name>;
extern builder::dyn_var<void* (string)> find_stack_var;
}

class runtime_value_resolver {
	static int resolver_counter;
public:		
	block::block::Ptr resolver = nullptr;
	typedef std::function<builder::dyn_var<rt::string>(builder::dyn_var<rt::string>)> handler_t;
        handler_t handler;
	std::string resolver_name;
	runtime_value_resolver(handler_t handler): handler(handler) {
		resolver_name = "xray_resolver_" + std::to_string(resolver_counter);
		resolver_counter++;
	}
	void gen_resolver(void) {
		if (!resolver) {
			resolver = builder::builder_context().extract_function_ast(handler, resolver_name);
			block::eliminate_redundant_vars(resolver);
		} 
        }
};


struct value_pair {
	std::string value;
	runtime_value_resolver* rvalue;
};

class xray_context {
	std::string current_anchor_name;
	int anchor_counter = 0;	
	int current_anchor_counter;
	int current_line_number;	

	// Source locations 
	// line->frame->source_loc
	std::vector<std::vector<source_loc>> source_loc_table;	

	// Identifiers/vars 
	// line->vname:value_pair
	std::vector<std::map<std::string, value_pair>> var_table;

	// scopeid->vname:value_pair
	std::vector<std::map<std::string, value_pair>> live_vars;

	const char* ident_char = "\t";	
	const char* debug_entry_section = "XRAY_entry";


public:
	xray_context();

	void reset_context(void);

	std::string begin_section(void);
	void end_section(void);

	void nextl(void);

	void push_source_loc(source_loc);

	void push_var_scope(void);
	void pop_var_scope(void);
	void create_var(std::string);	
	void delete_var(std::string);

	void update_var(std::string, std::string);
	void update_var(std::string, runtime_value_resolver&);

	void insert_live_vars();
	
	void set_var_here(std::string, std::string);
	void set_var_here(std::string, runtime_value_resolver&);

	void emit_function_info(std::ostream& oss);

private:
	// Emit time state and functions only
	std::vector<std::string> string_table;
	std::map<std::string, int> reverse_string_table;

	std::vector<std::tuple<int, int, int, int>> emit_source_list;
	std::vector<std::pair<int, int>> emit_source_table;
	
	std::vector<std::pair<int, int>> emit_var_table;
	std::vector<std::pair<int, std::pair<int, runtime_value_resolver*>>> emit_var_list;

	// Resolver stuff
	std::vector<runtime_value_resolver*> used_resolvers;
	std::vector<runtime_value_resolver*> emitted_resolvers;

	// functions
	int get_string_id(std::string);		
};



}

#endif
