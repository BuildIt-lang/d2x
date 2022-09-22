#include "xray/xray.h"
#include <iostream>

#define STR1(x)  #x
#define STR(x)  STR1(x)
#define BASE_DIR STR(BASE_DIR_X)

int main(int argc, char* argv[]) {

	std::cout << "#include <stdio.h>\n";
	std::cout << "#include \"xray_runtime/xray_runtime.h\"\n";

	xray::xray_context context;
	
	std::cout << context.begin_section();
	
	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 1, "main", 0});
	std::cout << "int main(int argc, char* argv[]) {" << std::endl;
	context.nextl();

	builder::dyn_var<char*(int*)> to_str(builder::with_name("std::to_string"));	

	xray::runtime_value_resolver r1 ([&](auto v) -> auto {
		//return "Value of " + v;
		builder::dyn_var<int*> addr = xray::rt::find_stack_var(v);
		return "Value of " + v + " = " + to_str(addr[0]);
	});

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 2, "main", 1});
	context.create_var("v1");
	context.update_var("v1", r1);
	context.set_var_here("v1", r1);
	std::cout << "\tint v1 = 21;" << std::endl;
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 3, "main", 2});
	std::cout << "\tprintf(\"Hello %d\\n\", v1);" << std::endl;	
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 4, "main", 3});
	std::cout << "\treturn 0;" << std::endl;
	context.nextl();

	context.push_source_loc({BASE_DIR "/samples/sample2.txt", 4, "main", 3});
	std::cout << "}" << std::endl;
	context.nextl();		

	context.emit_function_info(std::cout);
	context.end_section();

	return 0;
}
