#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include "GameInstance.hpp"

int main(int argc, char *argv[]) {
	std::cout << "Starting Game..." << std::endl;

	ph::GameInstance game;
	try {
		game.init();
		game.run();
	} catch (const std::exception& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
	}

	return 0;
}
