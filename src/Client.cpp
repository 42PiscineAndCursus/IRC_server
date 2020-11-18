#include "../inc/Client.hpp"

Client::Client(int socket)
	:socket(socket), count(0)
{
	ft_memset(buff, 0, sizeof(buff));
	mode = "-a+i-w-r-o-O-s";
	type = "unknown";
	servername = "ft_irc@42madrid.com";
	std::cout << "Unknown client " << socket << " created.\n";
}

Client::~Client()
{
	close(socket);
}

Client::Client(const Client &rhs)
	:socket(rhs.socket), info(rhs.info), mode(rhs.mode), type(rhs.type), \
			count(rhs.count)
{
	ft_memset(buff, 0, sizeof(buff));
}

void Client::resetBuffer(void)
{
	ft_memset(this->buff, 0, 512);
	this->count = 0;
}

int Client::getSocket() const
{
	return this->socket;
}

void Client::setIP(const std::string &ip)
{
	this->ip = ip;
}

int Client::setClient()
{
	if (nick.length() > 0 && username.length() > 0)
		type = "client";
	else
		return -1;
	// 클라이언트의 nick 과 user설정이 완료되면 client를 설정함
	make_prefix();
	prefix = std::string(":").append(nick);
	// 왜 make_prefix함수를 쓰는지 모르겠음
	// make_prefix에서 설정한 값은 prefix = std::string(":").append(nick);에서 사라짐
	prefix.append("!");
	prefix.append(username);
	prefix.append("@");
	prefix.append(ip);
	// prefix설정
	// nick = dakim0 user = dakim0인경우 호스트 ip = 169.254.44.86인경우
	// prefix = :dakim0!dakim0@169.254.44.86 로 설정됨
	return (0);
}

void Client::make_prefix()
{
	prefix.clear();
	prefix = std::string(":").append(nick);
	prefix.append("!~");
	prefix.append(username);
	// nick = dakim0 user = dakim0인 경우
	// prefix = :dakim0!~dakim0으로 설정됨
	return;
}
