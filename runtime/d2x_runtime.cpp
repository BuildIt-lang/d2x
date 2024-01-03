#include "d2x_runtime/d2x_runtime.h"
#include "d2x/utils.h"
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#define UNW_LOCAL_ONLY
#include <libunwind.h>


namespace d2x {
namespace runtime {

std::vector<d2x_function_header*> *registered_function_headers = nullptr;


static void* last_ip = NULL;
static void* last_sp = NULL;
static struct d2x_context last_ctx;
static int current_frame_index = 0;
static int config_list_offset = 2;

struct d2x_context find_context(void* ip, void* sp, void* bp, void* bx) {
	if (last_ip == ip && last_sp == sp) 
		return last_ctx;
	else {
		current_frame_index = 0;		
	}

	last_ip = ip;
	last_sp = sp;

	d2x_headers_init();

	struct d2x_context &ctx = last_ctx;

	ctx.rip = (uint64_t) ip;
	ctx.rsp = (uint64_t) sp;
	ctx.rbp = (uint64_t) bp;
	ctx.rbx = (uint64_t) bx;
	

	ctx.function = 0;
	ctx.header = nullptr;
	ctx.dli_fname = nullptr;
	ctx.load_offset = 0;	
	ctx.address_line = -1;	
	ctx.function_line = -1;	
	// First we will identify the function this IP belongs to
	Dl_info info;
	struct link_map *map = nullptr;
	if (!dladdr1((void*) ip, &info, (void**)&map, RTLD_DL_LINKMAP)) {
		return ctx;
	}

	ctx.function = (uint64_t) info.dli_saddr;
	ctx.dli_fname = info.dli_fname;
	ctx.load_offset = (uint64_t) map->l_addr;


	Dwarf_Debug dbg;
	if (find_debug_info(ctx.dli_fname, &dbg)) {
		return ctx;
	}
	ctx.dbg = dbg;

	uint64_t adjusted_ip = ctx.rip - ctx.load_offset;
	int line_no = -1;
	const char* fname = NULL;
	std::string func_name, linkage_name;	
	find_line_info_with_dbg(dbg, adjusted_ip, &line_no, &fname, func_name, linkage_name);
	ctx.address_line = line_no;	
	ctx.src_filename = fname;
	
	if (line_no == -1)
		return ctx;
	
	// Now we will find the debug info for this function
	for (auto header: *registered_function_headers) {
		if (header->identified_filename == NULL || header->identified_line == -1) {
			int line_no = -1;
			const char* fname = NULL;
			uint64_t adjusted_ip = (uint64_t)header->function_addr - (uint64_t)ctx.load_offset;
			find_line_info(dbg, adjusted_ip, &line_no, &fname, func_name, linkage_name);
			header->identified_filename = fname;
			header->identified_line = line_no;
		}
		if (header->identified_filename == NULL || header->identified_line == -1)
			continue;
		if (strcmp(header->identified_filename, ctx.src_filename) == 0) {
			if (header->identified_line <= ctx.address_line 
				&& header->identified_line + header->source_table_len > ctx.address_line) {
				ctx.header = header;
				ctx.function_line = header->identified_line;
				break;
			}
		}
	}
		
	return ctx;
}
static std::string basename(const std::string& pathname)
{
    return {std::find_if(pathname.rbegin(), pathname.rend(),
                         [](char c) { return c == '/'; }).base(),
            pathname.end()};
}

std::string get_backtrace(struct d2x_context ctx) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";

	std::stringstream oss;
	int line_offset = ctx.address_line - ctx.function_line;

	struct d2x_source_stack stack = ctx.header->source_table[line_offset];
	struct d2x_source_loc *locs = ctx.header->source_list;
	const char** string_table = ctx.header->string_table;
	for (int i = 0; i < stack.stack_size; i++) {
		struct d2x_source_loc loc = locs[i + stack.stack_offset];
		if (loc.foffset != -1)
			oss << "#" << i << " in " << string_table[loc.function] << ":" << loc.foffset << " at " << basename(string_table[loc.filename]) << ":" << loc.linenumber << "\n";
		else
			oss << "#" << i << " in " << string_table[loc.function] << " at " << basename(string_table[loc.filename]) << ":" << loc.linenumber << "\n";
	}
	return oss.str();
}

std::string get_listing(struct d2x_context ctx) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";

	std::stringstream oss;
	int line_offset = ctx.address_line - ctx.function_line;
	struct d2x_source_stack stack = ctx.header->source_table[line_offset];
	struct d2x_source_loc *locs = ctx.header->source_list;
	const char** string_table = ctx.header->string_table;
	struct d2x_source_loc loc = locs[current_frame_index + stack.stack_offset];
	int linenumber = loc.linenumber;
	int bline = linenumber - config_list_offset;
	if (bline < 0)
		bline = 0;
	int eline = linenumber + config_list_offset;

	int cline = 0;
	std::string filename = string_table[loc.filename];

	std::ifstream ifs;
	ifs.open(filename, std::ifstream::in);	
	std::string line;	
	while (std::getline(ifs, line)) {
		cline++;
		if (cline < bline)
			continue;
		if (cline > eline)
			break;
		if (cline == linenumber)
			oss << ">" << cline << "\t" << line << "\n";
		else 
			oss << " " << cline << "\t" << line << "\n";
	}
	ifs.close();
	return oss.str();
}


std::string get_frame(struct d2x_context ctx, const char* update_frame) {
	int new_frame = -1;
	if (strcmp(update_frame, "")) {
		if (sscanf(update_frame, "%d", &new_frame) != 1) {
			new_frame = -1;	
		}
	}
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";

	std::stringstream oss;
	int line_offset = ctx.address_line - ctx.function_line;
	struct d2x_source_stack stack = ctx.header->source_table[line_offset];
	if (new_frame >= 0) {
		if (new_frame < stack.stack_size) 
			current_frame_index = new_frame;
		else 
			oss << "Warning: xFrame index " << new_frame << " is not valid. xFrame not updated\n";
	}
	struct d2x_source_loc *locs = ctx.header->source_list;
	const char** string_table = ctx.header->string_table;
	struct d2x_source_loc loc = locs[current_frame_index + stack.stack_offset];
	int linenumber = loc.linenumber;

	if (loc.foffset != -1) 
		oss << "#" << current_frame_index << " in " << string_table[loc.function] << ":" << loc.foffset << " at " << basename(string_table[loc.filename]) << ":" << loc.linenumber << "\n";
	else 
		oss << "#" << current_frame_index << " in " << string_table[loc.function] << " at " << basename(string_table[loc.filename]) << ":" << loc.linenumber << "\n";

	int cline = 0;
	std::string filename = string_table[loc.filename];
	std::ifstream ifs;
	ifs.open(filename, std::ifstream::in);	
	std::string line;	
	while (std::getline(ifs, line)) {
		cline++;
		if (cline == linenumber) {
			oss << cline << "\t" << line << "\n";
			break;
		}
	}
	ifs.close();
	return oss.str();	
}

std::string get_xfilename_ctx(struct d2x_context ctx) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";
	int line_offset = ctx.address_line - ctx.function_line;
	struct d2x_source_stack stack = ctx.header->source_table[line_offset];
	struct d2x_source_loc *locs = ctx.header->source_list;
	const char** string_table = ctx.header->string_table;
	struct d2x_source_loc loc = locs[current_frame_index + stack.stack_offset];

	std::string filename = string_table[loc.filename];
	return filename;	
}

static struct d2x_context* active_frame_ctx = nullptr;

static std::string find_die_name(Dwarf_Debug dbg, Dwarf_Die die) {
	char* name = NULL;
	Dwarf_Error de;
	Dwarf_Attribute at;
	if (dwarf_diename(die, &name, &de) == DW_DLV_OK) {		
		return name;
	}
	// There isn't name directly, let's check if there is AT_specification
	if (dwarf_attr(die, DW_AT_specification, &at, &de) == DW_DLV_OK) {
		Dwarf_Off off;
		Dwarf_Die spec;
		dwarf_global_formref(at, &off, &de);
		dwarf_offdie(dbg, off, &spec, &de);
		std::string ret = find_die_name(dbg, spec);
		if (ret != "")
			return ret;
	} 
	if (dwarf_attr(die, DW_AT_abstract_origin, &at, &de) == DW_DLV_OK) {
		Dwarf_Off off;
		Dwarf_Die spec;
		dwarf_global_formref(at, &off, &de);
		dwarf_offdie(dbg, off, &spec, &de);
		std::string ret = find_die_name(dbg, spec);
		if (ret != "")
			return ret;
	}
	return "";
}

#if 0
int dwarf_get_locdesc_entry_c(Dwarf_Loc_Head_c /*loclist_head*/,
   Dwarf_Unsigned    /*index*/,

   /* identifies type of locdesc entry*/
   Dwarf_Small    *  /*lle_value_out*/,
   Dwarf_Addr     *  /*lowpc_out*/,
   Dwarf_Addr     *  /*hipc_out*/,
   Dwarf_Unsigned *  /*loclist_count_out*/,
   Dwarf_Locdesc_c * /*locentry_out*/,
   Dwarf_Small    *  /*loclist_source_out*/, /* 0,1, or 2 */
   Dwarf_Unsigned *  /*expression_offset_out*/,
   Dwarf_Unsigned *  /*locdesc_offset_out*/,
   Dwarf_Error    *  /*error*/);
#endif

static void* decode_address_from_die(Dwarf_Debug dbg, Dwarf_Die die, uint64_t frame_base) {
	Dwarf_Attribute at;
	Dwarf_Error de;
	Dwarf_Unsigned no_of_elements = 0;
	Dwarf_Loc_Head_c loclist_head = 0;
	Dwarf_Unsigned op_count;
	Dwarf_Locdesc_c desc;
	
	Dwarf_Addr expr_low;
	Dwarf_Addr expr_high;


	// Dummy params
	Dwarf_Small d1;
	Dwarf_Small d4;
	Dwarf_Unsigned d5;
	Dwarf_Unsigned d6;

	int lres;
	if (dwarf_attr(die, DW_AT_location, &at, &de) == DW_DLV_OK) {
		lres = dwarf_get_loclist_c(at, &loclist_head, &no_of_elements, &de);
		if (lres != DW_DLV_OK)
			return NULL;
		//std::cout << "For variable, the location has no_of_elems = " << no_of_elements << std::endl;
		lres = dwarf_get_locdesc_entry_c(loclist_head, 0, &d1, &expr_low, &expr_high, &op_count, &desc, &d4, &d5, &d6, &de);

		if (op_count != 1 && op_count != 2) 
			return NULL;	

		uint64_t res;

		Dwarf_Small op;
		Dwarf_Unsigned opd1 = 0, opd2 = 0, opd3 = 0, opd4 = 0;
		Dwarf_Unsigned offsetforbranch = 0;
		dwarf_get_location_op_value_c(desc, 0, &op, &opd1, &opd2, &opd3, &offsetforbranch, &de);
		if (op != DW_OP_fbreg) 
			return NULL;
		res = frame_base + opd1;	

		if (op_count == 2) {
			dwarf_get_location_op_value_c(desc, 1, &op, &opd1, &opd2, &opd3, &offsetforbranch, &de);
			if (op != DW_OP_deref)
				return NULL;
			res = *(uint64_t*) res;
		}
		return (void*)res;
		
		//std::cout << "For variable, the location has no_of_elems = " << no_of_elements << std::endl;
	}
	return NULL;
}

static void* find_var_address_in_subprogram(Dwarf_Debug dbg, Dwarf_Die die, uint64_t pc, const char* varname, uint64_t frame_base) {	
	Dwarf_Half tag;
	Dwarf_Die child;
	Dwarf_Error de;
	Dwarf_Unsigned lopc, hipc;

	Dwarf_Half ret_form;
	enum Dwarf_Form_Class ret_class;

	if (dwarf_child(die, &child, &de) == DW_DLV_OK) {	
		while(1) {
			dwarf_tag(child, &tag, &de);
			if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
				std::string vname = find_die_name(dbg, child);
				if (vname == varname) {
					return decode_address_from_die(dbg, child, frame_base);
				}
			}					
			Dwarf_Die sibling;	
			if (dwarf_siblingof(dbg, child, &sibling, &de) != DW_DLV_OK)
				break;
			child = sibling;
		}
	}
	// We should also scan the lexical scopes if we haven't found anything
	if (dwarf_child(die, &child, &de) == DW_DLV_OK) {	
		while(1) {
			dwarf_tag(child, &tag, &de);
			if (tag == DW_TAG_lexical_block) {
				return find_var_address_in_subprogram(dbg, child, pc, varname, frame_base);
			// TODO: Fix this to check only those lexical block that are live at the address range
			/*	
				if (dwarf_lowpc(child, &lopc, &de) == DW_DLV_OK) {
					if (!(dwarf_highpc_b(child, &hipc, &ret_form, &ret_class, &de) == DW_DLV_OK)) {
						hipc = ~0ULL;
					}
					if (ret_class == DW_FORM_CLASS_CONSTANT) 
						hipc += lopc;
					if (pc >= lopc && pc < hipc) {
						// This is the function
						return find_var_address_in_subprogram(dbg, child, pc, varname, frame_base);
					}
			
				} else {
					
				}
			*/

			}					
			Dwarf_Die sibling;	
			if (dwarf_siblingof(dbg, child, &sibling, &de) != DW_DLV_OK)
				break;
			child = sibling;
		}
	}
	
	return NULL;
}

static void* find_var_address_in_die(Dwarf_Debug dbg, Dwarf_Die die, uint64_t pc, const char* varname, uint64_t frame_base) {
	Dwarf_Half tag;
	Dwarf_Error de;
	Dwarf_Unsigned lopc, hipc;
	Dwarf_Half ret_form;
	enum Dwarf_Form_Class ret_class;
	dwarf_tag(die, &tag, &de);
	if (tag == DW_TAG_subprogram) {
		if (dwarf_lowpc(die, &lopc, &de) == DW_DLV_OK) {
			if (!(dwarf_highpc_b(die, &hipc, &ret_form, &ret_class, &de) == DW_DLV_OK)) {
				hipc = ~0ULL;
			}
			if (ret_class == DW_FORM_CLASS_CONSTANT) 
				hipc += lopc;
			if (pc >= lopc && pc < hipc) {
				// This is the function
				return find_var_address_in_subprogram(dbg, die, pc, varname, frame_base);
			}
		}
	} else {
		Dwarf_Die child;
		if (dwarf_child(die, &child, &de) == DW_DLV_OK) {
			while (1) {
				void* ret = find_var_address_in_die(dbg, child, pc, varname, frame_base);
				if (ret != NULL)
					return ret;
				Dwarf_Die sibling;
				if (dwarf_siblingof(dbg, child, &sibling, &de) != DW_DLV_OK)
					break;
				child = sibling;
			}
		}
	}
	return NULL;
}

static void* find_var_loc(struct d2x_context ctx, const char* varname) {

	void* ret_val = NULL;

	unw_cursor_t cursor, cursor_next;
	unw_context_t context;
	context.uc_mcontext.gregs[REG_RBP] = ctx.rbp;
	context.uc_mcontext.gregs[REG_RIP] = ctx.rip;
	context.uc_mcontext.gregs[REG_RSP] = ctx.rsp;
	context.uc_mcontext.gregs[REG_RBX] = ctx.rbx;
	unw_init_local(&cursor, &context);
	cursor_next = cursor;
	unw_step(&cursor_next);	

	unw_word_t sp_next;
	unw_get_reg(&cursor_next, UNW_REG_SP, &sp_next);

	// We have obtained the base register
	// Now to find the address of the variable
	uint64_t adjusted_ip = (uint64_t)ctx.rip - (uint64_t)ctx.load_offset;
	Dwarf_Die cu_die = find_cu_die(ctx.dbg, adjusted_ip);
	if (cu_die == NULL) 
		goto cleanup;	
	
	ret_val = find_var_address_in_die(ctx.dbg, cu_die, adjusted_ip, varname, sp_next);
	


cleanup:
	if (cu_die != NULL)
		dwarf_dealloc(ctx.dbg, cu_die, DW_DLA_DIE);
	reset_cu(ctx.dbg);
	
	return ret_val;	
}

std::string get_fvl(struct d2x_context ctx, const char* varname) {
	std::stringstream oss;
	oss << "&" << varname << " = " << find_var_loc(ctx, varname) << std::endl;
	return oss.str();
}
// Should only be called from the rtv_handler
namespace rtv {
	void* find_stack_var(std::string varname) {
		return find_var_loc(*active_frame_ctx, varname.c_str());
	}
}

std::string get_vars(struct d2x_context ctx, const char* varname) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";
	
	std::stringstream oss;

	int line_offset = ctx.address_line - ctx.function_line;
	struct d2x_var_stack stack = ctx.header->var_table[line_offset];
	struct d2x_var_entry *vars = ctx.header->var_list;
	const char** string_table = ctx.header->string_table;
	
	int tofind = 0;	
	if (strcmp(varname, ""))
		tofind = 1;
	int found = 0;

	for (int i = 0; i < stack.stack_size; i++) {
		if (tofind) {
			if (strcmp(string_table[vars[stack.stack_offset + i].varname], varname) == 0) {
				if (vars[stack.stack_offset + i].varvalue != -1)
					oss << string_table[vars[stack.stack_offset + i].varname] << " = " << string_table[vars[stack.stack_offset + i].varvalue] << "\n";	
				else {
					auto varname = string_table[vars[stack.stack_offset + i].varname];
					active_frame_ctx = &ctx;
					auto func = (std::string (*)(std::string))vars[stack.stack_offset + i].rvarvalue;
					oss << varname << " = " << func(varname) << std::endl;
					active_frame_ctx = nullptr;
				}
				found = 1;
				break;
			}
		} else 
			oss << (i+1) << ". " << string_table[vars[stack.stack_offset + i].varname] << "\n";
	}	
	if (tofind == 1 && found == 0) {
		oss << "xVar " << varname << " not found at current location\n";
	}
	return oss.str();
}

static bool ends_with(const std::string& value, const std::string& ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static bool compare_paths(std::string spec_path, std::string path) {
	if (spec_path[0] == '/') {
		return spec_path == path;
	} else {
		return ends_with(path, spec_path);
	}
}

static std::vector<std::pair<std::string, int>> find_all_breaks(struct d2x_context ctx, std::string spec_filename, int spec_line_no) {
	std::vector<std::pair<std::string, int>> to_ret;

	for (auto header: *registered_function_headers) {
		if (header->identified_filename == NULL || header->identified_line == -1)
			continue;	
		// For each header iterate through each line and check if it has the source_spec at the "top" of 
		// the extended stack
		
		struct d2x_source_loc *locs = ctx.header->source_list;
		const char** string_table = ctx.header->string_table;

		for (int line_no = 0; line_no < header->source_table_len; line_no++) {
			struct d2x_source_stack stack = ctx.header->source_table[line_no];
			struct d2x_source_loc loc = locs[0 + stack.stack_offset];
			// This is the extended source of the top of the stack 
			// for the generated line of code
			int linenumber = loc.linenumber;
			std::string filename = string_table[loc.filename];
			if (compare_paths(spec_filename, filename) && linenumber == spec_line_no) {
				to_ret.push_back(std::make_pair(header->identified_filename, header->identified_line + line_no));
			}
		}
	}
	return to_ret;	
}

static std::vector<std::pair<std::string, int>> break_points_records;
static std::vector<std::vector<std::pair<std::string, int>>> break_points_map;
static std::vector<int> break_points_status;

std::string get_break(struct d2x_context ctx, const char* source_spec, std::ostream &output_command_file) {
	std::stringstream oss;
	// There are two types of source_specs
	// 1. just line number - this takes the filename (full path) from the from the current ctx and the specified line number
	// 2. <filename>:<linenumber> - Filename can be a full path or just the filename and a line number
	std::string spec = source_spec;
	if (spec == "") {
		// Just list all the breakpoints
		oss << "Following breakpoints exist:" << std::endl;
		int index = 0;
		for (auto b: break_points_map) {
			if (break_points_status[index] == 3) {
				index++;
				continue;
			}
			oss << "#" << index << " [" << (break_points_status[index] == 1 ? "ENABLED": "DISABLED") << "] " 
				<< break_points_records[index].first << ":" << break_points_records[index].second << std::endl;
			index++;
		}
		return oss.str();
	}
	std::string filename;
	static char filename_s[1024];		
	int line_no;
	if (sscanf(source_spec, "%[^:]:%d", filename_s, &line_no) == 2) {
		filename = filename_s;
	} else if (sscanf(source_spec, "%d", &line_no) == 1) {
		filename = get_xfilename_ctx(ctx);
		if (filename == "") {
			oss << "Cannot identify extended stack information for current location, aborting!" << std::endl;
			return oss.str();
		}
	} else {
		oss << "Command requires a source spec of the form [<filename>:]<linenumber>" << std::endl;
		return oss.str();
	}
	
	std::vector<std::pair<std::string, int>> break_point_locs = find_all_breaks(ctx, filename, line_no);
	for (auto b: break_point_locs) {
		output_command_file << "break " << b.first << ":" << b.second << std::endl;
	}
	oss << "Inserting " << break_point_locs.size() << " breakpoints with ID: #" << break_points_map.size()  << std::endl;
	
	// Breakpoint status
	// 1 - Active
	// 2 - Disabled
	// 3 - Deleted

	break_points_status.push_back(1);
	break_points_map.push_back(std::move(break_point_locs));
	break_points_records.push_back(std::make_pair(filename, line_no));

	return oss.str();
}

std::string get_del(struct d2x_context ctx, const char* source_spec, std::ostream &output_command_file) {
	std::stringstream oss;
	// There is only one type of inputs for del
	// 1. Break point id of the form #id
	std::string spec = source_spec;
	if (spec == "") {
		oss << "Command requires a breakpoint id (#<id>). Run xbreak without any parameters to list all breakpoints" << std::endl;
		return oss.str();
	}
	int break_id = -1;
	if (sscanf(source_spec, "#%d", &break_id) != 1) {
		oss << "Command requires a breakpoint id (#<id>). Run xbreak without any parameters to list all breakpoints" << std::endl;
		return oss.str();
	}
	if (break_id < 0 || break_id >= break_points_records.size() || break_points_status[break_id] == 3) {
		oss << "ID #" << break_id << " is not a valid break point. Run xbreak without any parameters to list all breakpoints" << std::endl;
		return oss.str();
	}
	
	for (auto b: break_points_map[break_id]) {
		output_command_file << "clear " << b.first << ":" << b.second << std::endl;
	}
	oss << "Deleting " << break_points_map[break_id].size() << " breakpoints for ID: #" << break_id << std::endl;
	break_points_status[break_id] = 3;
	
	return oss.str();
}


void print_output(std::string s) {
	std::cout << s;
}

/* API functions to be invoked from the debugger */
namespace cmd {
void xbt(void* ip, void* sp, void* bp, void* bx) {
	print_output(get_backtrace(find_context(ip, sp, bp, bx)));
}
void xlist(void* ip, void* sp, void* bp, void* bx) {	
	print_output(get_listing(find_context(ip, sp, bp, bx)));
}
void xframe(void* ip, void* sp, void* bp, void* bx, const char* update) {
	print_output(get_frame(find_context(ip, sp, bp, bx), update));
}
void xvars(void* ip, void* sp, void* bp, void* bx, const char* varname) {
	print_output(get_vars(find_context(ip, sp, bp, bx), varname));
}
void xfvl(void* ip, void* sp, void* bp, void* bx, const char* varname) {
	print_output(get_fvl(find_context(ip, sp, bp, bx), varname));
}
// xbreak generates a command sequence to be executed besides the output
static char ret_command_buffer[1024];
const char* xbreak(void* ip, void* sp, void* bp, void* bx, const char* source_spec) {
	const char* filename = ".d2x.commands";	
	std::ofstream output_file;
	output_file.open(filename);			
	print_output(get_break(find_context(ip, sp, bp, bx), source_spec, output_file));
	// Clean up the command file after it executes
	output_file << std::endl << "shell rm -f " << filename << std::endl;
	output_file.close();
	sprintf(ret_command_buffer, "source %s", filename);
	return ret_command_buffer;
}
const char* xdel(void* ip, void* sp, void* bp, void* bx, const char* source_spec) {
	const char* filename = ".d2x.commands";	
	std::ofstream output_file;
	output_file.open(filename);			
	print_output(get_del(find_context(ip, sp, bp, bx), source_spec, output_file));
	// Clean up the command file after it executes
	output_file << std::endl << "shell rm -f " << filename << std::endl;
	output_file.close();
	sprintf(ret_command_buffer, "source %s", filename);
	return ret_command_buffer;
}
}
}
}

