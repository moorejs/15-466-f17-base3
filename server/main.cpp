#include "../shared/Server.hpp"

std::string port;

int main(int argc, char** argv) {
	port = argc > 1 ? argv[1] : "3490";

	Server server;

	return 0;
}