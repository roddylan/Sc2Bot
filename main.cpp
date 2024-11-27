#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "BasicSc2Bot.h"
#include "LadderInterface.h"

// LadderInterface allows the bot to be tested against the built-in AI or
// played against other bots
int main(int argc, char* argv[]) {
	int barrack_techlab_proportion = 1;
	int barrack_reactor_proportion = 1;
	for (int i = 0; i < argc; ++i) {
		std::string current_parameter(argv[i]);  // convert to std::string for ==
		if (current_parameter == "--barrack_techlab_proportion") {
			barrack_techlab_proportion = std::stoi(argv[i + 1]);
		}
		else if (current_parameter == "--barrack_reactor_proportion") {
			barrack_reactor_proportion = std::stoi(argv[i + 1]);
		}
	}
	Args args{ barrack_techlab_proportion, barrack_reactor_proportion };
	std::cout << "========= Bot-Specific Args =========" << std::endl;
	std::cout << " --barrack_techlab_proportion " << args.barrack_techlab_proportion << std::endl;
	std::cout << " --barrack_reactor_proportion " << args.barrack_reactor_proportion << std::endl;
	RunBot(argc, argv, new BasicSc2Bot(args), sc2::Race::Terran);
	return 0;
}