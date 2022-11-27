#include "d2x/d2x.h"

namespace d2x {

namespace rt {
const char string_t_name[] = "std::string";
builder::dyn_var<void* (string)> find_stack_var(builder::as_global("d2x::runtime::rtv::find_stack_var"));
}
int runtime_value_resolver::resolver_counter = 0;

void d2x_context::reset_context(void) {
	source_loc_table.clear();
	var_table.clear();
	live_vars.clear();
	live_vars.resize(1);			
	current_line_number = 0;
	var_table.resize(current_line_number + 1);
}

d2x_context::d2x_context() {
	reset_context();
}
std::string d2x_context::begin_section(void) {
	reset_context();
	current_anchor_name = "d2x_section_anchor_" + std::to_string(anchor_counter);
	current_anchor_counter = anchor_counter;
	anchor_counter++;

	nextl();
	return "static void " + current_anchor_name + "(void) {}\n";
}

void d2x_context::end_section(void) {
	current_anchor_name = "";
	current_line_number = -1;	
}

void d2x_context::nextl(void) {
	current_line_number++;
	var_table.resize(current_line_number + 1);
	source_loc_table.resize(current_line_number + 1);
	insert_live_vars();
}

void d2x_context::push_source_loc(source_loc loc) {
	// We are currently not insider any function
	if (current_line_number == -1) 
		return;
	var_table.resize(current_line_number + 1);
	source_loc_table.resize(current_line_number + 1);
	source_loc_table[current_line_number].push_back(loc);		
}

void d2x_context::push_var_scope(void) {
	live_vars.push_back(std::map<std::string, value_pair>());	
}
void d2x_context::pop_var_scope(void) {
	live_vars.pop_back();
}

void d2x_context::create_var(std::string vname) {
	value_pair v;
	v.value = "";
	v.rvalue = nullptr;
	live_vars.back()[vname] = v;
}

void d2x_context::delete_var(std::string vname) {
	live_vars.back().erase(vname);
}

void d2x_context::insert_live_vars(void) {
	auto &current = var_table.back();
	// Update values walking scopes backwards
	for (int i = (int)live_vars.size() - 1; i >= 0; i--) {
		auto &scope = live_vars[i];
		for (auto var = scope.begin(); var != scope.end(); var++) {
			if (current.find(var->first) == current.end()) {
				var_table.back()[var->first] = var->second;
			}
		}
	}
}

void d2x_context::update_var(std::string vname, std::string value) {
	// Update values walking scopes backwards
	for (int i = (int)live_vars.size() - 1; i >= 0; i--) {
		if (live_vars[i].find(vname) != live_vars[i].end()) {
			value_pair v;
			v.value = value;
			v.rvalue = nullptr;
			live_vars[i][vname] = v;
			break;
		}
	}
}
void d2x_context::update_var(std::string vname, runtime_value_resolver& r) {
	// Update values walking scopes backwards
	for (int i = (int)live_vars.size() - 1; i >= 0; i--) {
		if (live_vars[i].find(vname) != live_vars[i].end()) {
			value_pair v;
			v.value = "";
			v.rvalue = &r;
			live_vars[i][vname] = v;
			break;
		}
	}
}

void d2x_context::set_var_here(std::string vname, std::string value) {
	value_pair v;
	v.value = value;
	v.rvalue = nullptr;
	var_table.back()[vname] = v;
}

void d2x_context::set_var_here(std::string vname, runtime_value_resolver& resolver) {
	value_pair v;
	v.value = "";
	v.rvalue = &resolver;
	var_table.back()[vname] = v;
}


}
