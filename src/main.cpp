#include "../inc/ft_irc.hpp"

Server *pserv;
// 시그널 처리를 위해 서만 사용되는 값임

void sighandler(int signal)
{
	(void)signal;
	std::cout << pserv->getInfo() << std::endl;
	pserv->clear();
	std::cout << "\nSaliendo\n";
	exit(0);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	signal(SIGINT, sighandler);
	Server serv(argc, argv);
	pserv = &serv;
	serv.init_server();
	if (argc == 4)
		serv.serv_connect();
	serv.main_loop();
}
