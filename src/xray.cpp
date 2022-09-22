#include "xray/xray.h"

namespace xray {

namespace rt {
const char string_t_name[] = "std::string";
builder::dyn_var<void* (string)> find_stack_var(builder::with_name("xray::runtime::rtv::find_stack_var"));
}
int runtime_value_resolver::resolver_counter = 0;

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
std::string xray_context::begin_section(void) {
	reset_context();
	current_anchor_name = "xray_section_anchor_" + std::to_string(anchor_counter);
	current_anchor_counter = anchor_counter;
	anchor_counter++;

	nextl();
	return "static void " + current_anchor_name + "(void) {}\n";
}

void xray_context::end_section(void) {
	current_anchor_name = "";
	current_line_number = -1;	
}

void xray_context::nextl(void) {
	current_line_number++;
	var_table.resize(current_line_number + 1);
	source_loc_table.resize(current_line_number + 1);
	insert_live_vars();
}

void xray_context::push_source_loc(source_loc loc) {
	// We are currently not insider any function
	if (current_line_number == -1) 
		return;
	var_table.resize(current_line_number + 1);
	source_loc_table.resize(current_line_number + 1);
	source_loc_table[current_line_number].push_back(loc);		
}

void xray_context::push_var_scope(void) {
	live_vars.push_back(std::map<std::string, value_pair>());	
}
void xray_context::pop_var_scope(void) {
	live_vars.pop_back();
}

void xray_context::create_var(std::string vname) {
	value_pair v;
	v.value = "";
	v.rvalue = nullptr;
	live_vars.back()[vname] = v;
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
				var_table.back()[var->first] = var->second;
			}
		}
	}
}

void xray_context::update_var(std::string vname, std::string value) {
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
void xray_context::update_var(std::string vname, runtime_value_resolver& r) {
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

void xray_context::set_var_here(std::string vname, std::string value) {
	value_pair v;
	v.value = value;
	v.rvalue = nullptr;
	var_table.back()[vname] = v;
}

void xray_context::set_var_here(std::string vname, runtime_value_resolver& resolver) {
	value_pair v;
	v.value = "";
	v.rvalue = &resolver;
	var_table.back()[vname] = v;
}


}
