#include "xray_runtime/xray_runtime.h"
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#include <sstream>
#include <fstream>

namespace xray {
namespace runtime {

std::vector<xray_function_header*> *registered_function_headers = nullptr;


static std::map<std::string, Dwarf_Debug> debug_map;

int find_debug_info(const char* filename, Dwarf_Debug* ret) {
	std::string path = filename;
	if (debug_map.find(path) != debug_map.end()) {
		*ret = debug_map[path];
		return 0;
	}
	// Allocate a new debug map
	Dwarf_Debug to_ret;
	Dwarf_Error de;
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		return -1;
	}	
	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &to_ret, &de)) {
		close(fd);
		return -1;
	}
	debug_map[path] = to_ret;
	*ret = to_ret;
	return 0;	
}

int dbg_step_cu(Dwarf_Debug dbg) {
	Dwarf_Error de;
	Dwarf_Unsigned hl;
	Dwarf_Half st;
	Dwarf_Off off;
	Dwarf_Half as, ls, xs;
	Dwarf_Sig8 ts;
	Dwarf_Unsigned to, nco;
	Dwarf_Half ct;
	return dwarf_next_cu_header_d(dbg, true, &hl, &st, &off, &as, &ls, &xs, &ts, &to, &nco, &ct, &de);
}

Dwarf_Die find_cu_die(Dwarf_Debug dbg, uint64_t addr) {
	int ret;
	Dwarf_Error de;
	Dwarf_Die die, to_ret, ret_die;
	Dwarf_Unsigned lopc, hipc;
	Dwarf_Half tag;
	Dwarf_Half ret_form;
	enum Dwarf_Form_Class ret_class;
	while ((ret = dbg_step_cu(dbg)) == DW_DLV_OK) {
		die = NULL;
		while (dwarf_siblingof(dbg, die, &ret_die, &de) == DW_DLV_OK) {
			if (die != NULL)
				dwarf_dealloc(dbg, die, DW_DLA_DIE);
			die = ret_die;
			if (dwarf_tag(die, &tag, &de) != DW_DLV_OK)
				continue;
			if (tag == DW_TAG_compile_unit)
				break;		
		}
		if (ret_die == NULL) {
			if (die != NULL) {
				dwarf_dealloc(dbg, die, DW_DLA_DIE);
				die = NULL;		
			}
			continue;
		}
		if (dwarf_lowpc(die, &lopc, &de) == DW_DLV_OK) {
			if (!(dwarf_highpc_b(die, &hipc, &ret_form, &ret_class, &de) == DW_DLV_OK)) {
				hipc = ~0ULL;	
			}
			if (ret_class == DW_FORM_CLASS_CONSTANT)
				hipc += lopc;
			if (addr >= lopc && addr < hipc)
				return die;
		}
	}
}


void reset_cu(Dwarf_Debug dbg) {
	Dwarf_Error de;	
	int ret;
	while ((ret = dbg_step_cu(dbg)) != DW_DLV_NO_ENTRY) {
		if (ret == DW_DLV_ERROR) {
			return;
		}
	}
}

void find_line_info(Dwarf_Debug dbg, uint64_t addr, int *line_no, uint64_t faddr, int* fline_no) {
	*line_no = -1;
	*fline_no = -1;
	Dwarf_Die cu_die = find_cu_die(dbg, addr);
	Dwarf_Error de;
	if (cu_die == NULL)
		return;
	Dwarf_Signed lcount;
	Dwarf_Line *lbuf;
	Dwarf_Addr lineaddr, plineaddr;
	Dwarf_Unsigned lineno, plineno;	
		

	int ret = dwarf_srclines(cu_die, &lbuf, &lcount, &de);
	if (ret != DW_DLV_OK) {
		goto cleanup;
	}	
	int i;
	for (i = 0; i < lcount; i++) {
		if (dwarf_lineaddr(lbuf[i], &lineaddr, &de))
			goto cleanup;
		if (dwarf_lineno(lbuf[i], &lineno, &de))
			goto cleanup;

		if (faddr == lineaddr) {
			*fline_no = lineno;
		} else if (faddr < lineaddr && faddr > plineaddr) {
			*fline_no = plineno;
		}
		if (addr == lineaddr) {
			*line_no = lineno;	
			break;
		} else if (addr < lineaddr && addr > plineaddr) {
			*line_no = plineno;
			break;	
		}
		plineaddr = lineaddr;
		plineno = lineno;
	}

cleanup:
	if (cu_die != NULL)
		dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
	reset_cu(dbg);			
}

static uint64_t last_ip = ~0ull;
static uint64_t last_sp = ~0ull;
static struct xray_context last_ctx;
static int current_frame_index = 0;
static int config_list_offset = 2;

struct xray_context find_context(uint64_t ip, uint64_t sp) {
	if (last_ip == ip && last_sp == sp) 
		return last_ctx;
	else {
		current_frame_index = 0;		
	}

	last_ip = ip;
	last_sp = sp;

	xray_headers_init();

	struct xray_context &ctx = last_ctx;

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
	if (info.dli_saddr == nullptr) {
		return ctx;
	}
	ctx.function = (uint64_t) info.dli_saddr;
	ctx.dli_fname = info.dli_fname;
	ctx.load_offset = (uint64_t) map->l_addr;

	// Now we will find the debug info for this function
	for (auto header: *registered_function_headers) {
		if (header->function_addr == ctx.function) {
			ctx.header = header;
			break;
		}	
	}

	Dwarf_Debug dbg;
	if (find_debug_info(ctx.dli_fname, &dbg)) {
		return ctx;
	}

	uint64_t adjusted_ip = ip - ctx.load_offset;
	uint64_t adjusted_fip = ctx.function - ctx.load_offset;
	int line_no = 0;
	int fline_no = 0;
	find_line_info(dbg, adjusted_ip, &line_no, adjusted_fip, &fline_no);
	ctx.address_line = line_no;	
	ctx.function_line = fline_no;
	
	return ctx;
}

std::string get_backtrace(struct xray_context ctx) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";

	std::stringstream oss;
	int line_offset = ctx.address_line - ctx.function_line;

	struct xray_source_stack stack = ctx.header->source_table[line_offset];
	struct xray_source_loc *locs = ctx.header->source_list;
	char** string_table = ctx.header->string_table;
	for (int i = 0; i < stack.stack_size; i++) {
		struct xray_source_loc loc = locs[i + stack.stack_offset];
		oss << "#" << i << " in " << string_table[loc.function] << ":" << loc.foffset << " at " << string_table[loc.filename] << ":" << loc.linenumber << "\n";
	}
	return oss.str();
}

std::string get_listing(struct xray_context ctx) {
	if (ctx.header == nullptr)
		return "";
	if (ctx.address_line == -1 || ctx.function_line == -1)
		return "";

	std::stringstream oss;
	int line_offset = ctx.address_line - ctx.function_line;
	struct xray_source_stack stack = ctx.header->source_table[line_offset];
	struct xray_source_loc *locs = ctx.header->source_list;
	char** string_table = ctx.header->string_table;
	struct xray_source_loc loc = locs[current_frame_index + stack.stack_offset];
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

std::string get_frame(struct xray_context ctx, const char* update_frame) {
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
	struct xray_source_stack stack = ctx.header->source_table[line_offset];
	if (new_frame > 0) {
		if (new_frame < stack.stack_size) 
			current_frame_index = new_frame;
		else 
			oss << "Warning: xFrame index " << new_frame << " is not valid. xFrame not updated\n";
	}
	struct xray_source_loc *locs = ctx.header->source_list;
	char** string_table = ctx.header->string_table;
	struct xray_source_loc loc = locs[current_frame_index + stack.stack_offset];
	int linenumber = loc.linenumber;

	oss << "#" << current_frame_index << " in " << string_table[loc.function] << ":" << loc.foffset << " at " << string_table[loc.filename] << ":" << loc.linenumber << "\n";

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
void print_output(std::string s) {
	std::cout << s;
}

/* API functions to be invoked from the debugger */
namespace cmd {
void xbt(uint64_t ip, uint64_t sp) {
	print_output(get_backtrace(find_context(ip, sp)));
}
void xlist(uint64_t ip, uint64_t sp) {	
	print_output(get_listing(find_context(ip, sp)));
}
void xframe(uint64_t ip, uint64_t sp, const char* update) {
	print_output(get_frame(find_context(ip, sp), update));
}
}
}
}

