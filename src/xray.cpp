#include "xray/xray.h"

namespace xray {

void xray_context::reset_context(void) {
	source_loc_table.clear();
	var_table.clear();
	live_vars.clear();
	live_vars.resize(1);			
	current_line_number = 0;
	var_table.resize(current_line_number + 1);
}

xray_context::xray_context() {
	reset_context();
}
void xray_context::begin_function(std::string fname) {
	reset_context();
	current_function_name = fname;
}

void xray_context::end_function(void) {
	current_function_name = "";
	current_line_number = -1;	
}

void xray_context::nextl(void) {
	current_line_number++;
	var_table.resize(current_line_number + 1);
	insert_live_vars();
}

void xray_context::push_source_loc(source_loc loc) {
	source_loc_table.resize(current_line_number + 1);		
	source_loc_table[current_line_number].push_back(loc);		
}

void xray_context::push_var_scope(void) {
	live_vars.push_back(std::map<std::string, std::string>());	
}
void xray_context::pop_var_scope(void) {
	live_vars.pop_back();
}

void xray_context::create_var(std::string vname) {
	live_vars.back()[vname] = "";		
}

void xray_context::delete_var(std::string vname) {
	live_vars.back().erase(vname);
}

void xray_context::insert_live_vars(void) {
	auto &current = var_table.back();
	// Update values walking scopes backwards
	for (int i = (int)live_vars.size() - 1; i >= 0; i--) {
		auto &scope = live_vars[i];
		for (auto var = scope.begin(); var != scope.end(); var++) {
			if (current.find(var->first) == current.end()) {
				set_var_here(var->first, var->second);
			}
		}
	}
}

void xray_context::update_var(std::string vname, std::string value) {
	// Update values walking scopes backwards
	for (int i = (int)live_vars.size() - 1; i >= 0; i--) {
		if (live_vars[i].find(vname) != live_vars[i].end()) {
			live_vars[i][vname] = value;
			break;
		}
	}
}

void xray_context::set_var_here(std::string vname, std::string value) {
	var_table.back()[vname] = value;	
}



}
