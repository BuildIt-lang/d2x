#include "xray/xray.h"
#include <iostream>

#define STR1(x)  #x
#define STR(x)  STR1(x)
#define BASE_DIR STR(BASE_DIR_X)

int main(int argc, char* argv[]) {

	std::cout << "#include <stdio.h>\n";
	std::cout << "#include \"xray_runtime/xray_runtime.h\"\n";

	xray::xray_context context;
	
	context.begin_function("main");
	
	context.create_var("argc");
	context.create_var("argv");
	
	context.update_var("argc", "0");
	context.set_var_here("argc", "0");
	context.update_var("argv", "NULL");
	context.set_var_here("argv", "NULL");

	context.push_source_loc({BASE_DIR "/samples/sample1.txt", 1});
	std::cout << "int main(int argc, char* argv[]) {" << std::endl;

	context.nextl();
	context.push_source_loc({BASE_DIR "/samples/sample1.txt", 2});
	std::cout << "\tprintf(\"Hello\\n\");" << std::endl;
	
	context.nextl();
	context.push_source_loc({BASE_DIR "/samples/sample1.txt", 3});
	std::cout << "\treturn 0;" << std::endl;

	context.nextl();
	context.push_source_loc({BASE_DIR "/samples/sample1.txt", 3});
	std::cout << "}" << std::endl;
		

	context.emit_function_info(std::cout);
	context.end_function();

	return 0;
}
