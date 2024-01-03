#ifndef D2X_UTILS_H
#define D2X_UTILS_H

#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#include <sstream>
#include <fstream>

namespace d2x {
namespace util {

static void find_line_info_with_dbg(Dwarf_Debug dbg, uint64_t addr, int *line_no, const char** fname, 
	std::string &function_name, std::string &linkage_name);

int find_line_info(uint64_t addr, int* line_no, const char** filename, 
	std::string &function_name, std::string &linkage_name);

}
}


#endif
