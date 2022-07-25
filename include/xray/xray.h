#ifndef XRAY_H
#define XRAY_H
#include <string>
#include <vector>
#include <map>
#include <ostream>

namespace xray {

struct source_loc {
	std::string file;
	int line;
};

class xray_context {
	std::string current_function_name;
	int current_line_number;	

	// Source locations 
	// line->frame->source_loc
	std::vector<std::vector<source_loc>> source_loc_table;	

	// Identifiers/vars 
	// line->vname:vvalue
	std::vector<std::map<std::string, std::string>> var_table;
	// scopeid->vname:vvalue	
	std::vector<std::map<std::string, std::string>> live_vars;

	const char* ident_char = "\t";	
	const char* debug_entry_section = "XRAY_entry";
public:
	xray_context();

	void reset_context(void);
	void begin_function(std::string);
	void end_function(void);
	void nextl(void);

	void push_source_loc(source_loc);

	void push_var_scope(void);
	void pop_var_scope(void);
	void create_var(std::string);	
	void delete_var(std::string);
	void update_var(std::string, std::string);
	void insert_live_vars();
	
	void set_var_here(std::string, std::string);
	void emit_function_info(std::ostream& oss);

private:
	// Emit time state and functions only
	std::vector<std::string> string_table;
	std::map<std::string, int> reverse_string_table;

	std::vector<std::pair<int, int>> source_list;

	std::vector<std::pair<int, int>> source_table;

	// functions
	int get_string_id(std::string);		
};


}

#endif
