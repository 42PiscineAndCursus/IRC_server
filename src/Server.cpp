#include "../inc/Server.hpp"

Server::Server(int argc, char **argv)
	:  msgofday("HELLO_MADRID"), creationtime("02/09/2019 09:00:00 GMT+1")
{
	int i;
	int j;
	std::string argv1;

	if (argc < 3 || argc > 4)
	{
		std::cout << ("Error:\nArguments. USAGE: ");
		std::cout << "./ircserv [host:port_network:password_network] <port> <password>\n";
		exit(EXIT_FAILURE);
	}
	this->port = std::string(argv[argc - 2]);
	this->password = std::string(argv[argc - 1]);

	if (argc == 4)
	{
		argv1 = std::string(argv[1]);
		i = argv1.find(':');
		this->host = argv1.substr(0, i);
		j = argv1.find(":", i + 1);
		this->port_network = argv1.substr(i + 1, j - i - 1);
		this->password_network = argv1.substr(j + 1, argv1.length() - 1);
	}
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
}

Server::~Server()
{
}

void Server::clear()
{
	for (std::vector<Client*>::iterator it= clients.begin();
		it!= clients.end(); )
	{
		if ((*it)->type == "client")
		{
			Message msg(":loop QUIT", *it);
			++it;
			quit(msg);

		}
		else
		{
			++it;
		}

	}
}

std::string Server::getIp(void)
{
	return IPADDRESS;
}

void Server::init_server()
{
	struct addrinfo	hints;
	struct addrinfo	*ai = NULL;
	struct addrinfo *p;
	int				yes = 1;
	int 			rv;

	fcntl(STDOUT_FILENO, F_SETFL, O_NONBLOCK);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	ft_memset(&hints, 0, sizeof hints);
	//hints.ai_family = PF_INET; //IPV4
	hints.ai_family = AF_UNSPEC; //IPV4 + IPV6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(0, this->port.c_str(), &hints, &ai)) != 0)
		ft_perror(strerror(rv));
	this->ip = getIp();
	for (p = ai; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
			continue;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			continue;
		}
		break;
	}
	if (p == NULL)
		ft_perror("Error: failed to bind.");
	if (listen(listener, 10) == -1)
		ft_perror("Error: Could not listen.");
	print("Server connected.");
	print(getInfo());
	FD_SET(listener, &master);
	if (port == SSLPORT)
		ssl_init();
	this->fdmax = this->listener;
	freeaddrinfo(ai);
}

void Server::serv_connect()
{
	int sockfd;
	std::string buff;
	struct addrinfo hints;
	struct addrinfo *ai = NULL;
	struct addrinfo *p;
	int rv;

	ft_memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(this->host.c_str(), this->port_network.c_str(), &hints, &ai)) != 0)
		ft_perror(strerror(rv));
	for (p = ai; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0)
			continue;
		if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            ft_perror("Server to server: could not connect");
            continue;
        }
		break;
	}
	if (p == NULL)
        ft_perror("failed to connect\n");
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	FD_SET(sockfd, &master);
	if (sockfd > fdmax)
		fdmax = sockfd;
	Client *tmp = new Client(sockfd);
	tmp->setIP(host);
	tmp->hopcount = 1;
	tmp->type = "server";
	clients.push_back(tmp);
}


void Server::main_loop()
{
	// 서버와의 통신을 수신하는 함수
	// 서버에 대한 접속 요청 및 메시지 전송을 수신하는 함수
	if (port != SSLPORT)
	{
		// TODO 왜 ssl port를 6669로 하였는가?
		// ssl포트가 아닌경우
		while(42)
		{
			serv_select();
			for (int i = 0; i <= fdmax; ++i)
			{
				// 파일디스크립터에 변화가 생긴경우
				if (FD_ISSET(i, &read_fds))
				{
					// listener의 경우 처음 소켓을 열었을 때 얻은 파일디스크립터임
					// getIP함수에 socket으로부터 파일디스트립터를 listener에 저장하는 부분 있음
					if (i == listener)
					{
						// listener의 fd에 변경이 있는경우
						// => 새로 연결하는 경우
						new_connection();
					}
					else
					{
						// listener가 아닌 다른 fd에 변경이 있는경우
						// => 새로운 연결이 아닌경우
						receive(i);
					}
				}
			}
		}
	}
	else
	{
		// ssl포트인 경우
		// TODO ssl 통신 확인하지 않음
		while(42)
		{
			serv_select();
			for (int i = 0; i <= fdmax; ++i)
			{
				if (FD_ISSET(i, &read_fds))
				{
					if (i == listener)
						ssl_new_connection();
					else
						ssl_receive(i);
				}
			}
		}
	}

}

void Server::new_connection()
{
	// 클라이언트의 접속 요청을 처리하는 함수
	// 클라이언트의 접속 요청을 받아 새로운 fd 생성 후 새로운 fd를 이용하여 클라이언트 객체 생성 함수
	// 이 함수의 경우 fd를 설정하는 부분, fd를 이용하여 클라이언트를 설정하는 부분으로 나누어
	// 모듈화 시키는것이 더 효율적이지 않을까 하는 생각이 듬
	int					newfd;
	struct sockaddr_in	remoteaddr;
	socklen_t			addrlen = sizeof(struct sockaddr_in);

	ft_memset(&remoteaddr, 0, sizeof(remoteaddr));
	ft_memset(&(remoteaddr.sin_addr), 0, sizeof(remoteaddr.sin_addr));
	newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
	// 클라이언트의 접속 요쳥을 받아 해당하는 클라이언트와 통신하는 전용 fd생성
	if (newfd == -1)
	{
		// 에러가 발생한 경우
		std::cerr << ("Error: Accept.") << std::endl;
		return ;
	}
	fcntl(newfd, F_SETFL, O_NONBLOCK);
	// fcntl : 이미 fd로 열려있는 파일을 제어하는데 사용 fcntl(int fd, int cmd, long arg)
	// F_SETFL : arg로 fd의 플래그를 재설정
	// O_NONBLOCK : I/O작업이 완료 될 수 없으면 바로 에러를 리턴함
	// => I/O를 기다리느라 프로그램이 멈추는것을 막을 수 있음
	FD_SET(newfd, &master);
	// 새로 얻은 파일디스크립터를 master그룹으로 설정
	if (newfd > fdmax)
		fdmax = newfd;
	// 새로 얻은 파일디스크립터를 이용하여 fdmax를 업데이트
	Client *tmp = new Client(newfd);
	tmp->setIP(IPADDRESS);
	clients.push_back(tmp);
	// 새로 얻은 파일디스크립터이용하여 클라이언트 객체를 생성하고, 벡터에 저장
	sendmsg(tmp->socket, "NOTICE * : PLEASE LOG IN");
	// 새로 연결된 클라이언트에 메시지 전송
	std::cout << "New connection on socket " << newfd << std::endl;
}

void	Server::serv_select()
{
	// select를 이용하여 그룹화된 fd에 변화가 생겼는지 체크
	struct timeval timeout;

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	read_fds = master;
	if (select(fdmax + 1, &read_fds, STDIN_FILENO, NULL, &timeout) == -1)
		ft_perror("Error: could not call select.");
}

std::string
Server::getInfo(void) const
{
	std::string ret;

	ret.append("Host: ");
	ret.append(this->host);
	ret.append(" Version: 0.1beta - Protocol 2.10 - 42 Madrid");
	ret.append(" ");
	ret.append(this->creationtime);
	return (ret);
}

void Server::sendmsg(int socket, std::string const &str)
{
	// socket에 해당하는 클라이언트에 메시지 전송
	std::string tmp;

	tmp = str.substr(0, 510);
	// irc의 메시지는 510캐릭터를 넘으면 안됨
	// RFC 1459 Page 7
	// section7 참고

	// ==========================
	// 개행(\n)은 두가지 기능의 조합임
	// 1. 줄 바꾸기
	// 2. 커서를 줄 맨 앞으로 가져다 두기

	// 유닉스 운영체제 상에서 \n은 위의 두가지 기능의 조합이지만
	// 일부 다른 운영체제에서는 위의 두가지 기능이 \n \r으로 분리되어 있는 경우가 있음
	// 따라서 운영체제간 통신에서 개행을 수행할 경우 \r\n을 입력하는것이 안전함

	tmp.append("\r");
	// append : 문자열 끝에 인자를 붙임
	if (str.find("\n") == std::string::npos)
		tmp.append("\n");

	// 위의 조건문을 작성한 이유를 잘 모르겠음
	// str에 \n이 하나라도 있는경우 위의 조건문에 의해 문자열 마지막에 \r\n이 붙지 않음
	// 따라서 개행이 제대로 작동하지 않음
	// sendmsg함수 사용한 경우를 보면, 모든 경우를 확인하지는 못했지만, 개행을 넣어 사용하는경우를 확인하지 못했음
	// ==========================

	if (FD_ISSET(socket, &master))
	{
		// 그룹화 되어있지 않은 fd로 부터 보호하기위해 조건문을 작성한음것으로 보임
		if (port == SSLPORT)
		{
			// ssl통신인 경우
			// TODO  ssl통신 확인하지 않음
			SSL *ssl = getClient(socket)->ssl;
			int ret = SSL_write(ssl, tmp.c_str(), ft_strlen(tmp.c_str()));
			if (ret <= 0)
				print("ssl write error");
		}
		else if ((send(socket, tmp.c_str(), ft_strlen(tmp.c_str()), 0)) == -1)
			std::cerr << "Error sending info to " << socket << std::endl;
		// send : 데이터 전송을 위해 사용되는 함수 / -1 인경우 에러
		// send(int socket, const void *buffer, size_t length, int flags)
		// flag를 제외한 나머지 write와 같음
		// flag가 0인경우 send함수는 write함수와 동일하게 작동함
	}
	else
		std::cerr << "Sendmsg: error: FD " << socket << " is not set\n";
}

void Server::receive(int socket)
{
	// 소켓으로 부터 전송되는 메시지 수신 및 처리
	int rec = 0;
	Client *tmp;

	tmp = getClient(socket);
	// 소켓에 해당하는 클라이언트를 받아옴
	// getClient는 null을 리턴할 수 있음
	// main_loop에서 소켓에 해당하는 클라이언트가 없는경우를 보호하고 있기 때문에
	// 현재의 코드에서 tmp에 NULL이 담길 가능성은 전무함
	if ((rec = recv(socket, &tmp->buff[tmp->count], 512 - tmp->count, 0)) >= 0)
	{
		// TODO 왜 이런 방식으로 코드를 작성하였는지 이해 못함
		tmp->count += rec;
	}
	if (rec == -1)
	{
		print(strerror(rec));
		std::cerr << "Error: recv returned -1" << std::endl;
	}
	else if (rec == 0)
	{
		// recv함수의 리턴값이 0인경우는 소켓연결이 끊어진것을 의미함
		std::cerr << "Error: socket " << socket << " disconnected\n";
		close(socket);
		// fd를 닫음
		deleteClient(socket);
		// 소켓에 해당하는 client객체를 삭제함
		FD_CLR(socket, &master);
		// 소켓을 master그룹에서 삭제
	}
	if (std::string(tmp->buff).find("\n") != std::string::npos || tmp->count >= 512)
	{
		std::cout << "Received " << tmp->buff << " from " << tmp->socket << "\n";
		// 위의 cout 코드 때문에 메시지 길이가 개행 포함 479이상인 경우 프로그램이 멈춤
		Message msg(tmp->buff, tmp);
		// string을 Message로 파싱함
		exec(msg);
		// 파싱된 메시지 처리
		tmp->resetBuffer();
		// client의 buffer 및 count초기화
	}
}

void Server::receive_noexec(int socket)
{
	int rec = 0;
	Client *tmp;

	tmp = getClient(socket);
	if ((rec = recv(socket, &tmp->buff[tmp->count], 512 - tmp->count, 0)) >= 0)
	{
		tmp->count += rec;
	}
	if (rec == -1)
	{
		print(strerror(rec));
		std::cerr << "Error: recv returned -1" << std::endl;
	}
	else if (rec == 0)
	{
		std::cerr << "Error: socket " << socket << " disconnected\n";
		close(socket);
		deleteClient(socket);
		FD_CLR(socket, &master);
	}
	if (std::string(tmp->buff).find("\n") != std::string::npos || tmp->count >= 512)
	{
		std::cout << "Received " << tmp->buff << " from " << tmp->socket<< "\n";
		tmp->resetBuffer();
	}
}

Client *Server::getClient(int socket)
{
	// socket에 해당하는 클라이언트를 받아옴
	std::vector<Client*>::iterator it = clients.begin();
	std::vector<Client*>::iterator itt = clients.end();

	while (it != itt)
	{
		if ((*it)->socket == socket)
			return *it;
		++it;
	}
	return NULL;
}

Channel &Server::getChannel(std::string const &ch)
{
	for (std::vector<Channel>::iterator it = channels.begin();
		it != channels.end(); it++)
	{
		if ((*it).name == ch)
			return *it;
	}
	return *channels.end();
}

Channel &Server::getOtherChannel(std::string const &ch)
{
	if (otherchannels.size() < 1)
		return *otherchannels.end();
	for (std::vector<Channel>::iterator it = otherchannels.begin();
		it != otherchannels.end(); it++)
	{
		if (it->name == ch)
			return *it;
	}
	return *otherchannels.end();
}

void Server::deleteClient(int socket)
{
	// 소켓에 해당하는 client를 삭제함
	std::vector<Client*>::iterator it = clients.begin();
	std::vector<Client*>::iterator itt = clients.end();

	while (it != itt)
	{
		if ((*it)->socket == socket)
		{
			delete *it;
			clients.erase(it);
		}
		++it;
	}
}

std::vector<Channel>::iterator Server::exists(std::string &channel)
{
	std::vector<Channel>::iterator it = this->channels.begin();
	while (it != this->channels.end())
	{
		if (it->name == channel)
			return it;
		++it;
	}
	return this->channels.end();
}

void Server::forward(std::string const &str)
{
	for (std::vector<Client*>::iterator it = clients.begin();
		it != clients.end(); ++it)
	{
		if ((*it)->type == "server" || (*it)->type == "preserver")
		{
			sendmsg((*it)->socket, str);
		}
	}
}

void Server::broadcast(
		std::string const &orig, std::string const &rpl, std::string const &msg)
{
	std::string arr[] = {":", orig, " ", rpl, " ", msg, "NULL"};
	std::string str = buildString(arr);
	for (std::vector<Client*>::iterator it = clients.begin();
		it != clients.end(); ++it)
	{
		if ((*it)->type == "server")
			sendmsg((*it)->socket, str);
	}
}



void Server::tochannel(std::string const &channel, std::string const &msg, int orig)
{
	Channel c = getChannel(channel);
	if (c.name.length() > 0)
	{
		for (std::map<std::string, std::string>::iterator it = c.nicks.begin();
			it != c.nicks.end(); ++it)
		{

			int socket = getClient((*it).first)->socket;
			if (socket != orig)
			{
				sendmsg(socket, msg);
			}
		}
	}
/* 	if (getClient(orig)->type == "server")
	{
		print("forwarded privmsg to channel");
		return;
	}
	Channel oc = getOtherChannel(channel);
	if (oc.name.length() > 1)
	{
		print("forwarding privmsg to channel");
		std::string fwd[] = {msg, "NULL"};
		forward(buildString(fwd));
	} */
}

void Server::tochannel(Channel &channel, Message &msg)
{
	if (msg.orig->type == "client")
	{
		std::string fwd[] = {msg.orig->prefix, " ", msg.msg, "NULL"};
		for (std::map<std::string, std::string>::iterator it = channel.nicks.begin();
			it != channel.nicks.end(); ++it)
		{
			int socket = getClient((*it).first)->socket;
			if (socket != msg.socket)
			{
				sendmsg(socket, buildString(fwd));
			}
		}
		forward(buildString(fwd));
	}
}

void Server::not_params(Message &msg)
{
	std::string words[] = {this->ip, " ", ERR_NEEDMOREPARAMS, " ", \
	msg.orig->nick, " ", msg.command, " :Not enough parameters", "NULL" };
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::no_such_channel(Message &msg)
{
	std::string words[] = {":", this->ip, " ", ERR_NOSUCHCHANNEL, " ", \
	msg.orig->nick, " ", *msg.params.begin(), " :No such channel", "NULL" };
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::no_such_server(Message &msg)
{
	std::string words[] = {":", this->ip, " ", ERR_NOSUCHSERVER, " ", \
	msg.orig->nick, " ", *msg.params.begin(), " :No such server", "NULL" };
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::not_in_channel(Message &msg)
{
	std::string words[] = {":", this->ip, " ", ERR_NOTONCHANNEL, " ", \
	msg.orig->nick, " ", *msg.params.begin(), " :You're not on that channel",
	"NULL" };
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::not_operator(Message &msg)
{
	std::string words[] = {":", this->ip, " ", ERR_CHANOPRIVSNEEDED, " ", \
	msg.orig->nick, " ", *msg.params.begin(), " :You're not an operator of this channel",
	"NULL" };
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::no_such_nick_channel(Message &msg)
{
	std::string words[] = {":", ip, " ", ERR_NOSUCHNICK, " ", msg.orig->nick, \
	" ", *msg.params.begin(), " :No such nick/channel", "NULL"};
	sendmsg(msg.socket, buildString(words));
}

void Server::unknown_command(Message &msg)
{
	if (msg.orig->type == "server")
		return;
	std::string strs[] = {":", ip, " ", ERR_UNKNOWNCOMMAND, " ",\
	msg.orig->nick, " ", msg.command, " :Unknown command", "NULL"};
	std::string reply = buildString(strs);
	sendmsg(msg.orig->socket, reply);
}


void Server::welcome(const Client &cli)
{
	std::string newuser("NEWUSER ");
	newuser.append(cli.nick);
	forward(newuser);
	std::string words[] = {":", ip, " ", RPL_WELCOME, " ", cli.nick, \
	" :Welcome to our ft_IRC from asegovia and darodrig ", cli.nick, "NULL"};
	sendmsg(cli.socket, buildString(words));
	std::string words2[] = {":", ip, " ", RPL_YOURHOST, " ", cli.nick, \
	" :Your host is ", host, "[", ip, "/", port, "], running version ft_irc-0.5", "NULL"};
	sendmsg(cli.socket, buildString(words2));
	std::string words3[] = {":", ip, " ", RPL_CREATED, " ", cli.nick, \
	" :This server was created Mon Nov 4 2019 at 09:00:00 UTC + 1", "NULL"};
	sendmsg(cli.socket, buildString(words3));
	std::string words4[] = {":", ip, " ", RPL_MYINFO, " ", cli.nick, \
	" ft_irc-0.5 ", "NULL"}; //Aqui van los posibles modos de usuario.
	sendmsg(cli.socket, buildString(words4));
	std::string words5[] = {":", ip, " ", "005", " ", cli.nick, \
	" CHARSET=ascii", "NULL"}; //Aqui van otras cosas soportadas, longitud maxima de nick...
	sendmsg(cli.socket, buildString(words5));
	Client *cll = getClient(cli.nick);
	lusers(cll);
	motd(cll);
}

void Server::sendusers(Message &msg)
{

	ft_wait(WAITTIME);
	int users = 0;
	for (std::vector<Client*>::iterator it = this->clients.begin();
		it != this->clients.end(); ++it)
	{
		if ((*it)->type == "client")
			++users;
	}
	if (users > 0)
	{
		std::string users = "NEWUSER ";
		for (std::vector<Client*>::iterator it = clients.begin();
			it != clients.end(); ++it)
		{
			if ((*it)->type == "client")
			{
				users.append((*it)->nick);
				users.append(" ");
			}
		}
		sendmsg(msg.socket, users);
	}

	ft_wait(WAITTIME);
	int oth = 0;
	for (std::vector<std::string>::iterator it = this->otherclients.begin();
		it != this->otherclients.end(); ++it)
	{
		++oth;
	}
	if (oth > 0)
	{
		std::string others = "NEWUSER ";
		for (std::vector<std::string>::iterator it = otherclients.begin();
			it != otherclients.end(); ++it)
		{
			others.append(*it);
			others.append(" ");
		}
		sendmsg(msg.socket, others);
	}
}

void Server::sendchannels(Message &msg)
{
	if (channels.size() != 0)
	{
		for (std::vector<Channel>::iterator it = channels.begin();
			it != channels.end(); ++it)
		{
			if ((it)->name.at(0) == '&')
				continue;
			std::string users = "ADDCHANNEL ";
			users.append(it->name);
			users.append(" ");
			for (std::map<std::string, std::string>::iterator itt = it->nicks.begin();
				itt != it->nicks.end(); ++itt)
				{
					users.append(itt->first);
					users.append(" ");
				}
			ft_wait(WAITTIME);
			sendmsg(msg.socket, users);
		}
	}
	if (otherchannels.size() != 0)
	{
		for (std::vector<Channel>::iterator it = otherchannels.begin();
			it != otherchannels.end(); ++it)
		{
			std::string users = "ADDCHANNEL ";
			users.append(it->name);
			users.append(" ");
			for (std::map<std::string, std::string>::iterator itt = it->nicks.begin();
				itt != it->nicks.end(); ++itt)
				{
					users.append(itt->first);
					users.append(" ");
				}
			ft_wait(WAITTIME);
			sendmsg(msg.socket, users);
		}
	}
}

void Server::partother(std::string const &nick)
{
	for (std::vector<Channel>::iterator it = otherchannels.begin();
		it!= otherchannels.end(); ++it)
	{
		it->eraseClient(nick);
	}
}

void Server::quitother(std::string const &nick)
{
	for (std::vector<std::string>::iterator it = otherclients.begin();
		it != otherclients.end(); ++it)
	{
		if (*it == nick)
		{
			otherclients.erase(it);
			break;
		}
	}
	partother(nick);
}

bool Server::isServer(std::string const &nick)
{
	if (nick == port)
		return 1;
	for (std::vector<Client*>::iterator it = clients.begin();\
		it != clients.end(); ++it)
	{
		if ((*it)->nick == nick && (*it)->type == "server")
			return 1;
	}
	return 0;
}
