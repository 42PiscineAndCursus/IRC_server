#include "../inc/Server.hpp"

void Server::pass(Message &msg)
{
	// PASS 메시지 처리하는 함수
	// RFC 2812 p 9
	// 서버와의 연결에 필요한 패스워드 전송 메시지
	// 항상 패스워드 먼저 전송을 해야함
	// 유저 설정 및 닉네임 설정전에 패스워드를 입력하여야함
	// command : PASS
	// parameters :로패스워드
	// numberic replies : ERR_NEEDMOREPARAMS, ERR_ALREADYREGISTRED
	// ERR_NEEDMOREPARAMS : 클라이언트에서 전송한 메시지에 충분한 파라미터가 없는경우 리턴
	// 예 : "<command> :Not enough parameters"
	// ERR_ALREADYREGISTRED : 이미 등록된 내용들을 재설정하려고 하는경우 리턴 (예 : 패스워드 또는 유저 디테일)
	if (getClient(msg.socket)->type != "unknown")
		return;
	// PASS워드를 이미 입력하였다면, 함수를 종료시킴
	// 위의 조건문에서 ERR_ALREADYREGISTRED를 전송하도록 설정해야하지 않을까 생각함
	if (msg.params.size() > 0)
	{
		if (*msg.params.begin() == this->password)
		{
			// 전송받은 암호와 실제 암호가 같은경우
			print("Password accepted");
			sendmsg(msg.orig->socket, "NOTICE :Password accepted.");
			getClient(msg.socket)->type = "accepted";
			// 성공 메시지를 리턴하고 클라이언트 타입을 변경함
		}
		else
			sendmsg(msg.socket, "NOTICE :Incorrect password.");
	}
	// 위의 조건문에 들어가지 못한경우 ERR_ALREADYREGISTRED을 전송하도록 설정해야하지 않을까 생각함
}

Client* Server::getClient(std::string const &nick)
{
	std::vector<Client*>::iterator it = clients.begin();

	while (it != clients.end())
	{
		if ((*it)->nick == nick)
			return *it;
		++it;
	}
	return NULL;
}

std::string Server::getOtherClient(std::string const &nick)
{
	// 서버에 직접연결되어있지 않은 클라이언트 찾는 함수
	for (std::vector<std::string>::iterator it = otherclients.begin(); it != otherclients.end();
		it++)
	{
		if (*it == nick)
		{
			return nick;
		}
	}
	std::string ret("");
	return ret;
}

Client* Server::getClientByUser(std::string const &user)
{
	std::vector<Client*>::iterator it = clients.begin();

	while (it != clients.end())
	{
		if ((*it)->username == user)
			return *it;
		++it;
	}
	return NULL;
}

void Server::new_nick(Message &msg)
{
	if (msg.params.size() < 1)
	{
		// 파라미터가 충분하지 않은경우 ERR_NEEDMOREPARAMS에러를 리턴함
		// 다만 nick메시지는 ERR_NEEDMOREPARAMS를 리턴하지 않음
		// 이 경우에는 ERR_NONICKNAMEGIVEN를 리턴해야함
		not_params(msg);
		return;
	}
	if (getClient(*msg.params.begin()) != NULL)
	{
		// 기존의 닉네임이 있는경우
		// ERR_NICKNAMEINUSE : 이미 존재하는 닉네임으로 바꾸려고 하는경우
		// "<nick> :Nickname is already in use"
		std::string words[] = {":", this->ip, " ", ERR_NICKNAMEINUSE,\
		" * ", *msg.params.begin(), " :Nickname is already in use.", "NULL" };
		sendmsg(msg.orig->socket, buildString(words));
		return;
	}
	msg.orig->nick = *msg.params.begin();
	// 닉네임 설정
	std::cout << msg.orig->nick << " ----> +nick" << std::endl;
	if (msg.orig->setClient() == 0)
		welcome(*msg.orig);
	// setClient함수를 이용하여 클라이언트의 타입을 설정함
}

void Server::re_nick(Message &msg)
{
	// nick을 재등록하는 경우
	if (msg.orig->nick == *msg.params.begin())
	{
		// 바꾸려는 NICK과 바꾸려는 NICK이 다른경우 변경할 수 없음
		return;
	}
	if (getClient(*msg.params.begin()) != NULL)
	{
		// 바꾸려는 닉네임이 다른 클라이언트의 닉네임과 같은경우
		std::string words[] = {":", this->ip, " ", ERR_NICKNAMEINUSE,\
		" * ", *msg.params.begin(), " :Nickname is already in use.", "NULL" };
		sendmsg(msg.orig->socket, buildString(words));
		return;
	}
	for (std::vector<Channel>::iterator it = channels.begin();
		it != channels.end(); ++it)
		it->change_nick(msg.orig->nick, *msg.params.begin());
	// 클라이언트가 들어가있는 체널에 nick변경사항을 반영
	getClient(msg.orig->nick)->nick = *msg.params.begin();
	getClient(msg.orig->nick)->make_prefix();
	// 닉네임을 재 등록하는 경우 prefix가 :dakim!~dakim1을 바뀜
	// TODO 왜 여기서 make_prefix가 선언되는지 모르겠음
	std::cout << msg.orig->nick << " -> +change_nick" << std::endl;
}

void Server::nick(Message &msg)
{
	// NICK 메시지 처리하는 함수
	// RFC 2812 p 9
	// 사용자의 닉네임을 설정하거나 이미 설정한 닉네임을 변경하는 메시지
	// 클라이언트가 보내는 메시지중 유일하게 prefix를 사용할 수 있음
	// prefix는 일반적으로 서버에게 메시지의 실제 위치를 알려주는 역할을 함
	// command : NICK
	// parameters : 닉네임
	// numberic replies : ERR_NONICKNAMEGIVEN, ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE, ERR_NICKCOLLISION, ERR_UNAVAILRESOURCE, ERR_RESTRICTED
	// NICK wiz : 닉네임이 설정되어있지 않은경우 닉네임을 wiz로 설정 / 이미 닉네임이 등록되어있는경우 닉네임을 wiz로 변경
	// :WiZ!jto@tolsun.oulu.fi NICK Kilroy : Wiz를 Kilroy로 바꾼다고 알려줌
	// :WiZ!jto@tolsun.oulu.fi 닉네임!유저@호스트

	// ERR_NONICKNAMEGIVEN : 닉네임에대한 파라미터가 주어지지 않은경우
	// ERR_ERRONEUSNICKNAME : 제대로된 형식의 닉네임이 주어지지 않은경우
	// ERR_NICKNAMEINUSE : 이미 사용하고있는 닉네임으로 닉네임을 변경하려는 경우
	// ERR_NICKCOLLISION : nickname collision을 발견한 경우 / 다른 서버에 등록된 닉네임을 사용하려는 경우
	// ERR_UNAVAILRESOURCE : nick delay mechanism에 의해 블록된 닉네임으로 바꾸려고 하는경우
	// TODO nick delay mechanism이 뭔지 모름
	// ERR_RESTRICTED : 제한된 연결인경우
	//  TODO ERR_RESTRICTED 뭔말인지 모르겠음
	if (msg.orig->type == "accepted")
	{
		// 클라이언트의 nick을 최초로 설정하는 경우
		// USER를 등록하지 않은경우 NICK을 재등록하면 여기로 들어옴
		new_nick(msg);
	}
	else if (msg.orig->type == "client")
	{
		// 클라이언트에서 nick을 재등록하는 경우
		re_nick(msg);
	}
	else if (msg.orig->type == "server")
		new_nick(msg);
	else if (msg.orig->type == "preserver")
	{
		new_nick(msg);
		msg.orig->type = "server";
		sendmsg(msg.orig->socket, std::string("NICK ").append(port));
		sendusers(msg);
		sendchannels(msg);
	}
}

void Server::user(Message &msg)
{
	// RFC 2812 과 RFC 1459가 다름

	// USER <username> <hostname> <servername> <realname>
	// RFC 1459

	// USER <user> <mode> <unused> <realname>
	// RFC 2812

	if (msg.orig->type != "accepted")
		return; // RPL USERALREADYREG
	if (msg.params.size() < 4)
		return;
	msg.orig->username = *msg.params.begin();
	msg.orig->hostname = *(++msg.params.begin());
	msg.orig->realname = *(++(++(++msg.params.begin())));
	// 필요한 파라미터를 클라이언트에 저장
	std::cout << msg.orig->username << std::endl;
	std::cout << msg.orig->hostname << std::endl;
	std::cout << msg.orig->realname << std::endl;
	std::cout << msg.orig->nick << " -> +user" << std::endl;
	if (msg.orig->setClient() == 0)
		welcome(*msg.orig);
	// 클라이언트의 타입을 변경
}

void Server::quit(Message &msg)
{
	if (msg.prefix.length() > 1 && msg.prefix != ":loop")
	{
		std::string toremove = *((msg.params.begin()));
		quitother(toremove);
		return;
	}
	for (std::vector<Channel>::iterator it = channels.begin();
		it != channels.end(); ++it)
	{
		if ((*it).isClient(msg.orig->nick))
		{
			std::string words[] = {msg.orig->prefix, " ", "QUIT :", "NULL"};
			(*it).eraseClient(msg.orig->nick);
			tochannel((*it).name, buildString(words), msg.orig->socket);
		}
	}

	std::string str[] = {"Error: Closing link: ", msg.orig->ip, " (Client quit)", "NULL"};
	sendmsg(msg.orig->socket, buildString(str));
	std::string fwd[] = {
		":", ip, " QUIT ", msg.orig->nick
		,"NULL"
	};
	forward(buildString(fwd));
	close(msg.orig->socket);
	FD_CLR(msg.orig->socket, &master);

	delete msg.orig;
	if (msg.prefix.length() < 2)
		clients.erase(std::find(clients.begin(), clients.end(), msg.orig));
}

void Server::privmsg(Message &msg)
{
	if (msg.params.size() < 1)
	{
		std::string arr[] = {
			":", ip, " ", ERR_NOTEXTTOSEND, " ", msg.orig->nick,
			" :No text to send", "NULL"
		};
		sendmsg(msg.orig->socket, buildString(arr));
		return;
	}
	std::string dest = *msg.params.begin();
	Client *tmp = getClient(dest);
	Channel &ch = getChannel(dest);
	if (tmp != NULL)
	{ //PRIVATE MESSAGE
		std::string tosend = msg.orig->prefix;
		tosend.append(" ");
		tosend.append(msg.msg);
		sendmsg(tmp->socket, tosend);
		return;
	}
	else if (exists(dest) != channels.end())
	{ //CHANNEL MESSAGE
		if (msg.orig->type == "server" || ch.isClient(msg.orig->nick))
		{
			std::string arr[] = {
			msg.orig->prefix, " ", msg.msg, "NULL"};
			tochannel(ch.name, buildString(arr), msg.orig->socket);
		}
		else
		{ //not in channel
			std::string arr[] = {
				":", ip, " ", ERR_CANNOTSENDTOCHAN, " ", msg.orig->nick, " ",
				ch.name, " :Cannot send to nick/channel.", "NULL"
			};
			sendmsg(msg.orig->socket, buildString(arr));
			return;
		}
		for (std::vector<Channel>::iterator it = otherchannels.begin();
		it != otherchannels.end(); it++)
		{
			if (it->name == dest)
			{
				if (msg.orig->type == "server")
					return;
				std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
				forward(buildString(rpl));
				return;
			}
		}
		if (ch.name.length() > 2)
		{//CANAL EXISTE EN OTRO SERVIDOR
			if (msg.orig->type == "server")
				return;
			std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
			forward(buildString(rpl));
			return;
		}
		else {
			std::string words[] = {":", ip, " ", ERR_NOSUCHNICK, " ", msg.orig->nick, \
			" ", dest, " :No such nick/channel", "NULL"};
			sendmsg(msg.socket, buildString(words));
		}
		return;
	}
	if (msg.msg.length() > 8)
	{
		std::string other = getOtherClient(dest);
		if (other != "")
		{//USUARIO EXISTE EN OTRO SERVER
			if (msg.orig->type == "server")
				return;
			std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
			forward(buildString(rpl));
			return;
		}
		std::string words[] = {":", ip, " ", ERR_NOSUCHNICK, " ", msg.orig->nick, \
		" ", dest, " :No such nick/channel", "NULL"};
		sendmsg(msg.socket, buildString(words));
	}
}

void Server::notice(Message &msg)
{
	// notice메시지 처리하는 함수

	// NOTICE : PRIVMSG랑 비슷하게 사용됨
	// command : NOTICE
	// parameters: <msgtarget> <text>
	// PRIVMSG와의 차이점 : NOTICE메세지에 대한 응답으로써 automatic replies가 보내어지면 안됨
	if (msg.params.size() < 1)
		return;
	// 파라미터가 충분하지 않은경우 함수를 종료
	if ((int)msg.msg.find("NOTICE :Password accepted") != -1)
	{
		// 상대방 서버의 비밀번호 입력을 성공하였을 때 작동
		std::string srv[] = {"SERVER ", port, " 0 0 info", "NULL"};
		sendmsg(msg.socket, buildString(srv));
		return;
	}
	if ((int)msg.msg.find("PLEASE") != -1)
	{
		// 서버간의 연겨에서 사용함
		// 서버에 접속하게 되면 상대방 서버에서 "NOTICE * : PLEASE LOG IN"를 보내게 됨
		// 상대방 서버에 접속하기위해 비밀번호를 입력함
		std::string pass[] = {
		"PASS ", password_network, "NULL"
		};
		sendmsg(msg.socket, buildString(pass));
		return;
	}
	std::string dest = *msg.params.begin();
	Client *tmp = getClient(dest);
	Channel &ch = getChannel(dest);
	// msgtarget을 통하여 channel 또는 client를 가져옴
	if (tmp != NULL)
	{ //PRIVATE MESSAGE
		// 클라이언트에 notice를 보냄
		// nick만 등록되어있는 클라이언트에게도 메시지를 보냄
		// " NOTICE dakim0 sdfsdfsf" : user가 등록되어있지 않은경우 메시지 전송
		// ":dakim0!dakim0@169.254.44.86 NOTICE dakim0 123123" : user가 등록되어 있는 경우 메시지 전송
		std::string tosend = msg.orig->prefix;
		tosend.append(" ");
		// user로 등록되어있지 않은경우 위의 코드는 비활성화 되어야함
		tosend.append(msg.msg);
		sendmsg(tmp->socket, tosend);
		return;
	}
	else if (exists(dest) != channels.end())
	{ //CHANNEL MESSAGE
		// exists는 채널들이 들어가있는 벡터에서 dest와 일치하는 값을 찾아 리턴함
		// 채널을 찾은경우
		if (msg.orig->type == "server" || ch.isClient(msg.orig->nick))
		{
			// 서버로부터 온 메시지인경우 또는
			// 메시지를 보낸 사람이 해당하는 채널에 참석하고 있는경우
			// 메시지 전송
			std::string arr[] = {
			msg.orig->prefix, " ", msg.msg, "NULL"};
			tochannel(ch.name, buildString(arr), msg.orig->socket);
			// 채널에 참석하고있는 모두에게 메시지 전송함
		}
		else
		{ //not in channel
			// 메시지를 보낸 사람이 해당하는 채널에 참석하고 있지 않은경우
			std::string arr[] = {
				":", ip, " ", ERR_CANNOTSENDTOCHAN, " ", msg.orig->nick, " ",
				ch.name, " :Cannot send to nick/channel.", "NULL"
			};
			sendmsg(msg.orig->socket, buildString(arr));
			return;
		}
		for (std::vector<Channel>::iterator it = otherchannels.begin();
		it != otherchannels.end(); it++)
		{
			if (it->name == dest)
			{
				if (msg.orig->type == "server")
					return;
				// 메시지가 서버로부터 온경우는 무시함
				std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
				forward(buildString(rpl));
				// server와 연결된 클라이언트중 server또는 preserver인 클라이언트에 모두 메시지를 보냄
				return;
			}
		}
		// server의 경우 멤버 변수로 channels 벡터와 otherchannels 벡터 두가지를 가지고있음
		// 위의 getChannel은 channels에서 채널을 찾는것임
		// 채널에 참석하고있는 유저중 현재의 서버와 연결이 되어있는 경우는 channels에서 관리하는것으로 추정됨
		// 채널에 참석하고있는 유저중 현재의 서버와 연결이 되어있지 않은경우를 처리하기위해 otherchannels로 관리하는것 같음
		// 따라서 notice가 server에서 온 경우는 무시하는것 같음

		// TODO 서버 a b c가 각각 a-b-c로 연결되어있는경우
		// TODO 즉 서버 a와 c가 직접적으로 연결되어있지 않은경우
		// TODO a서버와 연결되어있는 유저 A c서버와 연결되어있는 유저 C가 있다고 가정할 때
		// TODO A와 C가 같은 채널에 있는경우 A가 notice를 채널에 전송하면 그 notice는 서버 c에 전송되는가?

		if (ch.name.length() > 2)
		{//CANAL EXISTE EN OTRO SERVIDOR -> 채널이 다른 서버에 있는경우로 번역됨
			if (msg.orig->type == "server")
				return;
			// 메시지가 서버로부터 온 경우는 무시함
			std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
			forward(buildString(rpl));
			return;
		}
		else {
			// 채널을 찾을 수 없는 경우
			std::string words[] = {":", ip, " ", ERR_NOSUCHNICK, " ", msg.orig->nick, \
			" ", dest, " :No such nick/channel", "NULL"};
			sendmsg(msg.socket, buildString(words));
		}
		return;
	}
	if (msg.msg.length() > 8)
	{
		// 서버가 직접적으로 연결되어있지 않은 클라이언트 처리
		// 클라이언트를 찾지 못하면 ""를 리턴함
		std::string other = getOtherClient(dest);
		if (other != "")
		{//USUARIO EXISTE EN OTRO SERVER -> 사용자가 다른 서버에 있는경우로 번역됨
			if (msg.orig->type == "server")
				return;
			std::string rpl[] = { msg.orig->prefix, " ", msg.msg, "NULL"};
			forward(buildString(rpl));
			return;
		}
		std::string words[] = {":", ip, " ", ERR_NOSUCHNICK, " ", msg.orig->nick, \
		" ", dest, " :No such nick/channel", "NULL"};
		sendmsg(msg.socket, buildString(words));
		// 서버와 직접 연결되어있지 않은 클라이언트에서 찾지 못한경우
	}
}

void Server::sendjoin(const std::string &name, const std::string &nick)
{
	std::string rpl[] = {
		"NEWJOIN ", name, " ", nick
		, "NULL"
	};
	forward(buildString(rpl));
}

void Server::join(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	std::vector<std::string> channels, keys;
	split(*msg.params.begin(), channels, ',');
	if (msg.params.size() > 1)
		split(*(++msg.params.begin()), keys, ',');
	for (size_t i = 0; i < channels.size(); ++i)
	{
		if (channels[i].at(0) != '&' && channels[i].at(0) != '#')
		{
			std::string words[] = {this->ip, " ", ERR_NOSUCHCHANNEL, " ", \
				msg.orig->nick, " ", channels[i], " :No such channel", "NULL" };
			sendmsg(msg.orig->socket, buildString(words));
		}
		else if (exists(channels[i]) == this->channels.end())
		{
			//Create channel
			Channel tmp(channels[i], msg.orig->nick);
			tmp.setPermissions(msg.orig->nick, "+o");
			this->channels.push_back(tmp);
			sendjoin(tmp.name, msg.orig->nick);
			if (i < keys.size())
				(--this->channels.end())->setKey(keys[i]);
			std::cout << "CANAL CREADO: " << tmp.name << std::endl;

			sendmsg(msg.orig->socket, tmp.sendUserList(ip, msg.orig->nick));
			std::string temp[] = {
				":", ip, " ", RPL_ENDOFNAMES, " ", msg.orig->nick, tmp.name,
				" :End of /NAMES list.", "NULL"
			};
			sendmsg(msg.orig->socket, buildString(temp));
		}
		else
		{
			//join channel
			Channel &tmp = *exists(channels[i]);
			if (tmp.key == "" || (i < keys.size() && keys[i] == tmp.key))
			{
				tmp.addClient(msg.orig->nick);
				sendjoin(tmp.name, msg.orig->nick);
				sendmsg(msg.orig->socket, tmp.sendUserList(ip, msg.orig->nick));
				std::string temp[] = {
					":", ip, " ", RPL_ENDOFNAMES, " ", msg.orig->nick, " ",
					tmp.name, " :End of /NAMES list.", "NULL"
				};
				sendmsg(msg.orig->socket, buildString(temp));
				std::string arr[] = {
					":", msg.orig->nick, "!", msg.orig->hostname, "@",
					msg.orig->ip, " JOIN ", tmp.name, "NULL"};
				tochannel(tmp.name, buildString(arr), -1);
			}
			else
			{
				std::string w[] = {":", this->ip, " NOTICE ", ERR_PASSWDMISMATCH, \
					" :Wrong password for the channel", "NULL"};
				sendmsg(msg.orig->socket, buildString(w));
			}
		}
	}
}

void	Server::part(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	if (msg.prefix.length() > 0)
	{
		Channel &c = getOtherChannel(*msg.params.begin());
		if (c.name.length() > 0)
		{
			c.eraseClient(*(++msg.params.begin()));
		}
		return;
	}
	std::vector<std::string> channels;
	split(*msg.params.begin(), channels, ',');
	for (size_t i = 0; i < channels.size(); ++i)
	{
		if (exists(channels[i]) == this->channels.end())
		{	//The channel doesn't exist.
			std::string words[] = {":", this->ip, " ", ERR_NOSUCHCHANNEL, " ", \
			msg.orig->nick, " ", channels[i], " :No such channel", "NULL" };
			sendmsg(msg.orig->socket, buildString(words));
		}
		else
		{

			Channel &tmp = *exists(channels[i]);
			if (!(tmp.isClient(msg.orig->nick)))
			{ //The channel exist, but you aren't in.
				std::string words[] = {":", this->ip, " ", ERR_NOTONCHANNEL, " ", \
				msg.orig->nick, " ", channels[i], " :You're not on that channel", "NULL" };
				sendmsg(msg.orig->socket, buildString(words));
			}
			else
			{   //You leave the channel, people are notified.
				std::string words[] = {msg.orig->prefix, " ", msg.command, \
				" ", channels[i], "NULL"};
				tochannel(channels[i], buildString(words), msg.orig->socket);
				sendmsg(msg.orig->socket, buildString(words));
				getChannel(channels[i]).eraseClient(msg.orig->nick);
				std::string fwd[] = {
					":", ip, " PART ", channels[i], " ", msg.orig->nick
					,"NULL"
				};
				forward(buildString(fwd));
			}
		}
	}
}

void	Server::server(Message &msg)
{
	if (msg.params.size() < 4)
		not_params(msg);
	else
	{
		Client *tmp = getClient(msg.orig->socket);
		int i = 0;
		std::list<std::string>::iterator it = msg.params.begin();
		while (it != msg.params.end())
		{
			if (i == 0)
				tmp->servername = *it;
			if (i == 1)
				tmp->hopcount = ft_atoi((*it).c_str());
			if (i == 2)
			{
				if ((tmp->token = *it) == "0")
				{
					tmp->type = "preserver";
					std::string srvrply[] = {
						"SERVER ft_irc_main 1 1 ",
						" no_info", "NULL" };
					sendmsg(tmp->socket, buildString(srvrply));
				}
				else if ((tmp->token = *it) == "1")
				{
					std::string srvrply[] = {
						"NICK ", port, " ", "NULL" };
					sendmsg(tmp->socket, buildString(srvrply));
				}
			}
			if (i > 2)
			{
				tmp->information.append(*it);
				if (it != (--msg.params.end()))
					tmp->information.append(" ");
			}
			++i;
			++it;
		}
		print("SERVER REGISTERED");
	}
}

void	Server::lusers(Client *cli)
{
	int users = 0, clients = 0, oper = 0, servers = 1, unknown = 0;
	int invisible = 0;

	for (std::vector<Client*>::iterator it = this->clients.begin();
		it != this->clients.end(); ++it)
	{
		if ((*it)->type == "unknown" || (*it)->type == "accepted" || \
			(*it)->type == "preserver" )
			++unknown;
		else if ((*it)->type == "client")
		{
			++clients;
			if ((int)(*it)->mode.find("-i") != -1)
				++users;
			else
				++invisible;
			if ((int)(*it)->mode.find("+o") != -1)
				++oper;
		}
		else
			++servers;
	}
	std::string lusers1[] = {":", ip, " ", RPL_LUSERCLIENT, " ", cli->nick,\
		" :", ft_itoa(users), " users and ", ft_itoa(invisible), \
		" invisible on ", ft_itoa(servers), " server(s)", "NULL"};
	sendmsg(cli->socket, buildString(lusers1));
	std::string lusers2[] = {":", ip, " ", RPL_LUSEROP, " ", cli->nick,\
		" :", ft_itoa(oper), " IRC Operators online", "NULL"};
	sendmsg(cli->socket, buildString(lusers2));
	std::string lusers3[] = {":", ip, " ", RPL_LUSERUNKNOWN, " ", cli->nick,\
		" :", ft_itoa(unknown), " unknown connection(s)", "NULL"};
	sendmsg(cli->socket, buildString(lusers3));
	std::string lusers4[] = {":", ip, " ", RPL_LUSERCHANNELS, " ", cli->nick,\
		" :", ft_itoa(channels.size()), " channel(s) formed", "NULL"};
	sendmsg(cli->socket, buildString(lusers4));
	std::string lusers5[] = {":", ip, " ", RPL_LUSERME, " ", cli->nick,\
		" :I have ", ft_itoa(clients), " clients and 1 server", "NULL"};
	sendmsg(cli->socket, buildString(lusers5));
}

void Server::ping(Message &msg)
{
	if (msg.params.size() < 1)
		not_params(msg);
	else
	{
		std::string rpl[] = {
			":", ip, " PONG ", ip, " :", "NULL"
		};
		std::string str = buildString(rpl);
		for (std::list<std::string>::iterator it = msg.params.begin();
			it != msg.params.end(); ++it)
		{
			str.append(*it);
			if (it != --msg.params.end())
				str.append(" ");
		}
		sendmsg(msg.orig->socket, str);
	}
}

void Server::motd(Client *orig)
{
	std::string begin[] = {
		":", ip, " ", RPL_MOTDSTART, " ", orig->nick,
		" -ft_irc Message of the Day - ", "NULL"
	};
	sendmsg(orig->socket, buildString(begin));
	std::string motd[4] =
	{
		"Welcome to ft_irc at 42 Madrid!",
		"This is an amazing project made by @Asegovia & @Darodrig",
		"Ft_irc is an IRC server restricted by the RFC standards.",
		"Have fun!"
	};
	for (int i = 0; i < 4; ++i)
	{
		std::string rpl[] = {
			":", ip, " ", RPL_MOTD, " ", orig->nick, " :- ", motd[i], "NULL"
		};
		sendmsg(orig->socket, buildString(rpl));
	}
	std::string endmotd[] = {
		":", ip, " ", RPL_ENDOFMOTD, " ", orig->nick,
		" :End of /MOTD command.", "NULL"
	};
	sendmsg(orig->socket, buildString(endmotd));
}

void	Server::list(Message &msg)
{
	std::string start[] = {
		":", ip, " ", RPL_LISTSTART, " ", msg.orig->nick,
		" :Start of /LIST command.", "NULL" };
	sendmsg(msg.socket, buildString(start));
	for (std::vector<Channel>::iterator it = channels.begin();
		it != channels.end(); ++it)
	{
		std::string rpl[] = {
			":", ip, " ", RPL_LIST, " ", msg.orig->nick, " ", it->name, " ",
			std::string(ft_itoa(it->nicks.size())), " :", it->topic
			,"NULL"
		};
		sendmsg(msg.socket, buildString(rpl));
	}
	std::string endlist[] = {
		":", ip, " ", RPL_LISTEND, " ", msg.orig->nick,
		" :End of /LIST command.", "NULL" };
	sendmsg(msg.socket, buildString(endlist));
}

void Server::topic(Message &msg)
{
	if (msg.params.size() < 2)
		not_params(msg);
	else
	{
		Channel &c = getChannel(*msg.params.begin());
		if (exists(*msg.params.begin()) == channels.end())
		{
			no_such_nick_channel(msg);
			return;
		}
		if (c.name.length() > 0)
		{
			if (c.isClient(msg.orig->nick) == false)
			{
				not_in_channel(msg);
				return;
			}
			if ((int)c.get_permissions(msg.orig->nick).find("+o") != -1)
			{
				c.topic = *(++msg.params.begin());
				std::string rpl[] = {
					":", ip, " ", RPL_TOPIC, " ", msg.orig->nick, " ", c.name, \
					" :", c.topic
					,"NULL"
				};
				tochannel(c.name, buildString(rpl), msg.socket);
				sendmsg(msg.socket, buildString(rpl));
			}
			else
				not_operator(msg);
		}
		else
			no_such_channel(msg);
	}
}

void Server::kick(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	if (exists(*msg.params.begin()) == this->channels.end())
	{
		no_such_channel(msg);
		return;
	}
	Channel &tmp = *exists(*msg.params.begin());
	std::string expulsed = *(++msg.params.begin());
	if (getClient(expulsed) == NULL)
		no_such_nick_channel(msg);
	else if (!(tmp.isClient(msg.orig->nick)))
		not_in_channel(msg);
	else if (!(tmp.isClient(expulsed)))
	{
		std::string rpl[] = {msg.orig->prefix, " ", ERR_USERNOTINCHANNEL, " ", msg.orig->nick, \
		" ", expulsed, " ", tmp.name, ":They aren't on that channel", "NULL"};
		sendmsg(msg.orig->socket, buildString(rpl));
	}
	else if ((int)tmp.get_permissions(msg.orig->nick).find("+o") != -1)
	{
		std::string words[] = {msg.orig->prefix, " KICK ", tmp.name, " ", \
		expulsed, ": ", expulsed, "NULL"};
		tochannel(tmp.name, buildString(words), msg.orig->socket);
		sendmsg(msg.orig->socket, buildString(words));
		tmp.eraseClient(expulsed);
	}
	else
		not_operator(msg);
}

void Server::oper(Message &msg)
{
	if (msg.params.size() < 2)
	{
		not_params(msg);
		return;
	}
	Client *cli = getClient(*msg.params.begin());
	if (!cli)
		no_such_nick_channel(msg);
	else
	{
		std::string perm = "+o";
		if (*(++msg.params.begin()) == OPSWD)
		{
			int pos = cli->mode.find("o");
			cli->mode.replace(pos - 1, 1, "+");
		}
		else
		{
			std::string rpl[] = {
				":", ip, " ", ERR_NOOPERPRIVILEGES, " ", *msg.params.begin(),
				" :No appropriate operator blocks were found for your host"
				, "NULL"
			};
			sendmsg(msg.socket, buildString(rpl));
		}
	}
}

void	Server::whois(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	Client *cli = getClient(*(msg.params.begin()));
	if (!cli)
	{
		no_such_nick_channel(msg);
		return;
	}
	std::string info[] = {
		":", ip, " ", RPL_WHOISUSER, " ", msg.orig->nick, " ", cli->nick, " ~",
		cli->username, " ", cli->ip, " * ", cli->realname
		,"NULL"
	};
	sendmsg(msg.socket, buildString(info));
	std::string chstr;
	for (std::vector<Channel>::iterator it = channels.begin(); it != channels.end();
		++it)
	{
		if (it->isClient(msg.orig->nick) && it->isClient(cli->nick))
		{
			if ((int)it->get_permissions(cli->nick).find("+o") != -1)
				chstr.append("@");
			chstr.append((*it).name);
			chstr.append(" ");
		}
	}
	std::string info2[] = {
		":", ip, " ", RPL_WHOISCHANNELS, " ", msg.orig->nick, " ", cli->nick,
		" :", chstr
		,"NULL"
	};
	sendmsg(msg.socket, buildString(info2));
	std::string info3[] = {
		":", ip, " ", RPL_WHOISSERVER, " ", msg.orig->nick, " ", cli->nick, " ",
		ip, " :Madrid, ES, EU"
		,"NULL"
	};
	sendmsg(msg.socket, buildString(info3));
	std::string info4[] = {
		":", ip, " ", RPL_WHOISSERVER, " ", msg.orig->nick, " ", cli->nick,
		" :End of /WHOIS list."
		,"NULL"
	};
	sendmsg(msg.socket, buildString(info3));
}

void	Server::who(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	Channel &c = getChannel(*msg.params.begin());
	if (c.name.length() > 0 && c.isClient(msg.orig->nick))
	{
		for (std::map<std::string, std::string>::iterator it = c.nicks.begin();
			it != c.nicks.end(); ++it)
		{
			std::string whorpl[19] = {
				":", ip, " ", RPL_WHOREPLY, " ", msg.orig->nick, " ", c.name, //8
				" ~", msg.orig->username, " ", msg.orig->ip, " ", msg.orig->servername,
				" ", msg.orig->nick, " H :0 ", msg.orig->nick, "NULL"
			};
			sendmsg(msg.socket, buildString(whorpl));
		}
	}
	std::string rpl[] = {
		":", ip, " ", RPL_ENDOFWHO, " ", msg.orig->nick, " ", *msg.params.begin(),
		" :End of /WHO list."
		,"NULL"
	};
	sendmsg(msg.socket, buildString(rpl));
}

void	Server::mode(Message &msg)
{
	if (msg.params.size() < 2)
	{
		not_params(msg);
		return;
	}
	if (msg.params.size() == 3)
	{
		Channel &c = getChannel(*msg.params.begin());
		if (c.name.at(0) == '#' || c.name.at(0) == '&')
		{
			if ((int)c.get_permissions(msg.orig->nick).find("+o") != -1)
				c.banned.push_back(*(++(++msg.params.begin())));
			if (c.isClient(msg.orig->nick))
			{
				c.setPermissions(*(++(++msg.params.begin())), *(++msg.params.begin()));

				std::string rpl[] = {
					msg.orig->prefix, "@", ip, " MODE ", c.name, " ", \
					*(++msg.params.begin()), " ", *(++(++msg.params.begin())), "!*@*"
					,"NULL"
				};
				tochannel(c.name, buildString(rpl), msg.orig->socket);
				sendmsg(msg.socket, buildString(rpl));
				if (*(++msg.params.begin()) == "+b")
					c.eraseClient(*(++(++msg.params.begin())));
			}
		}
	}
}
	// Deberíamos poner nombres a los servers para poder seleccionar de cual
	//queremoos sacar los parametros.
void Server::version(Message &msg)
{
	if (msg.params.size() > 0)
	{
		std::string &server = *msg.params.begin();
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	std::string words[] = {":", ip, " ", RPL_VERSION, " ", msg.orig->nick, \
	" :ft_irc v0.2 (20201015-42mad) ft_irc.42madrid.com", "NULL"};
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::info(Message &msg)
{
	if (msg.params.size() > 0)
	{
		std::string &server = *msg.params.begin();
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	std::string words[] = {":", ip, " ", RPL_INFO, " ", msg.orig->nick, \
	" :FT_IRC using RFC protocol", "NULL"};
	sendmsg(msg.orig->socket, buildString(words));
	std::string words2[] = {":", ip, " ", RPL_INFO, " ", msg.orig->nick, \
	" :You're using FT_IRC! For assistance, go comb yourself.", "NULL"};
	sendmsg(msg.orig->socket, buildString(words2));
	std::string words3[] = {":", ip, " ", RPL_INFO, " ", msg.orig->nick, \
	" :Copyright (c) 2020 ft_irc development team", "NULL"};
	sendmsg(msg.orig->socket, buildString(words3));
	std::string words4[] = {":", ip, " ", RPL_INFO, " ", msg.orig->nick, \
	" :info@42madrid.com", "NULL"};
	sendmsg(msg.orig->socket, buildString(words4));
	std::string w5[] = {":", ip, " ", RPL_ENDOFINFO, " ", msg.orig->nick, \
	" :End of /INFO list.", "NULL"};
	sendmsg(msg.orig->socket, buildString(w5));
}

void Server::time(Message &msg)
{
	if (msg.params.size() > 0)
	{
		std::string &server = *msg.params.begin();
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	std::string words[] = {":", this->ip, " ", RPL_TIME," ", msg.orig->nick,\
	" Compilation time | ", __TIME__, " ", __DATE__,\
	"NULL"};
	sendmsg(msg.orig->socket, buildString(words));
}

void Server::newuser(Message &msg)
{
	if (msg.orig->type != "server")
		unknown_command(msg);
	for (std::list<std::string>::iterator it = msg.params.begin();
		it != msg.params.end(); ++it)
	{
		print(std::string("+otherclient ").append(*it));

		otherclients.push_back(*it);
	}
}

void Server::addchannel(Message &msg)
{
	if (msg.orig->type != "server")
	{
		unknown_command(msg);
		return;
	}
	Channel tmp(*msg.params.begin());
	for (std::list<std::string>::iterator it = ++msg.params.begin();
		it != msg.params.end(); ++it)
	{
		tmp.nicks.insert(std::make_pair<std::string,std::string>(*it, "-o-b"));
	}
	otherchannels.push_back(tmp);
}

void	Server::newjoin(Message &msg)
{
	std::string name = *msg.params.begin();
	std::string nick = *(++msg.params.begin());

	for (std::vector<Channel>::iterator it = otherchannels.begin();
		it != otherchannels.end(); ++it)
	{
		if (name == (it)->name)
		{
			it->nicks.insert(std::make_pair<std::string,std::string>(nick, "-o-b"));
			print("New channel added");
			return;
		}
	}
	Channel tmp(name, nick);
	otherchannels.push_back(tmp);
}

void	Server::admin(Message &msg)
{
	if (msg.params.size() > 0)
	{
		std::string &server = *msg.params.begin();
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	std::string words[] = {":", ip, " ", RPL_ADMINME, " ", msg.orig->nick, \
	" :Administrative info abot ft_irc", "NULL"};
	sendmsg(msg.orig->socket, buildString(words));
	std::string words2[] = {":", ip, " ", RPL_ADMINLOC1, " ", msg.orig->nick, \
	" :You're using FT_IRC! For assistance, go comb yourself.", "NULL"};
	sendmsg(msg.orig->socket, buildString(words2));
	std::string words3[] = {":", ip, " ", RPL_ADMINLOC2, " ", msg.orig->nick, \
	" :For further assistance, you can go to google.com", "NULL"};
	sendmsg(msg.orig->socket, buildString(words3));
	std::string words4[] = {":", ip, " ", RPL_ADMINEMAIL, " ", msg.orig->nick, \
	" :info@42madrid.com", "NULL"};
	sendmsg(msg.orig->socket, buildString(words4));
}

void	Server::stats(Message &msg)
{
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	else if (msg.params.size() > 0)
	{
		std::string &server = *msg.params.begin();
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	if ((int)msg.orig->mode.find("+o") == -1)
	{
		not_operator(msg);
		return;
	}

	int users = 0, clients = 0, oper = 0, servers = 1, unknown = 0;
	int invisible = 0;

	for (std::vector<Client*>::iterator it = this->clients.begin();
		it != this->clients.end(); ++it)
	{
		if ((*it)->type == "unknown" || (*it)->type == "accepted" || \
			(*it)->type == "preserver" )
			++unknown;
		else if ((*it)->type == "client")
		{
			++clients;
			if ((int)(*it)->mode.find("-i") != -1)
				++users;
			else
				++invisible;
			if ((int)(*it)->mode.find("+o") != -1)
				++oper;
		}
		else
			++servers;
	}
	std::string lusers1[] = {":", ip, " ", RPL_STATSOLINE, " ", msg.orig->nick,\
		" :", ft_itoa(users), " users and ", ft_itoa(invisible), \
		" invisible on ", ft_itoa(servers), " server(s)", "NULL"};
	sendmsg(msg.orig->socket, buildString(lusers1));
	std::string lusers2[] = {":", ip, " ", RPL_STATSOLINE, " ", msg.orig->nick,\
		" :", ft_itoa(oper), " IRC Operators online", "NULL"};
	sendmsg(msg.orig->socket, buildString(lusers2));
	std::string lusers5[] = {":", ip, " ", RPL_STATSOLINE, " ", msg.orig->nick,\
		" :I have ", ft_itoa(clients), " clients", "NULL"};
	sendmsg(msg.orig->socket, buildString(lusers5));
	std::string l5[] = {":", ip, " ", RPL_STATSCOMMANDS, " ", msg.orig->nick,\
		" :NOTICE PRIVMSG PASS USER NICK MODE OPER LIST TOPIC PING PART", "NULL"};
	sendmsg(msg.orig->socket, buildString(l5));
	std::string l6[] = {":", ip, " ", RPL_STATSCOMMANDS, " ", msg.orig->nick,\
		" :INFO VERSION STATS ADMIN CONNECT", "NULL"};
	sendmsg(msg.orig->socket, buildString(l6));
	std::string l7[] = {":", ip, " ", RPL_ENDOFSTATS, " ", msg.orig->nick,\
		" :End of /STATS list.", "NULL"};
	sendmsg(msg.orig->socket, buildString(l7));
}

void	Server::trace(Message &msg)
{
	if (msg.params.size() < 1 || (int)msg.orig->mode.find("+o") == -1)
	{
		std::string rpl[] = {
		":", ip, " ", RPL_TRACEUSER, " ", msg.orig->nick, " User ",
		msg.orig->nick, "[~", msg.orig->username, "] ", "NULL"};
		sendmsg(msg.socket, buildString(rpl));
		std::string endoftrace[] = {
		":", ip, " ", RPL_ENDOFTRACE, " ", msg.orig->nick,
		" End of TRACE"
		,"NULL" };
		sendmsg(msg.socket, buildString(endoftrace));
		return;
	}
	std::string &server = *msg.params.begin();
	if (msg.params.size() > 0)
	{
		if (!isServer(server))
		{
			no_such_server(msg);
			return;
		}
	}
	std::string trc[] = { ":", ip, " ", RPL_TRACESERVER, " ", msg.orig->nick,\
	" :Trace from server on ", port, "NULL"};
	print(buildString(trc));
	sendmsg(msg.socket, buildString(trc));
	std::string trc2[] = { ":", ip, " ", RPL_TRACESERVER, " ",  msg.orig->nick,\
	" :Connections on this server: ", ft_itoa(clients.size()), "NULL"};
	sendmsg(msg.socket, buildString(trc2));
	for (std::vector<Client*>::iterator it = clients.begin(); it!= clients.end(); ++it)
	{
		if ((*it)->type == "client")
		{
			std::string rpl[] = {
				":", ip, " ", RPL_TRACEUSER, " ", msg.orig->nick, " User ",
				(*it)->nick, "[~", (*it)->username, "] "
				,"NULL"
			};
			sendmsg(msg.socket, buildString(rpl));
		}
		if ((*it)->type == "server")
		{
			std::string rpl2[] = {
				":", ip, " ", RPL_TRACESERVER, " ", msg.orig->nick, " Server ",
				(*it)->nick, "[~ft_irc] "
				,"NULL"
			};
			sendmsg(msg.socket, buildString(rpl2));
		}
	}
	std::string endoftrace[] = {
		":", ip, " ", RPL_ENDOFTRACE, " ", msg.orig->nick,
		" End of TRACE"
		,"NULL"
	};
	sendmsg(msg.socket, buildString(endoftrace));
}

void	Server::connect(Message &msg)
{
	if ((int)msg.orig->mode.find("+o") == -1)
	{
		not_operator(msg);
		return;
	}
	if (msg.params.size() < 1)
	{
		not_params(msg);
		return;
	}
	std::string portbck = this->port;
	this->port = *msg.params.begin();
	this->port_network = *(msg.params.begin()++);
	serv_connect();
	port = portbck;
	for (std::vector<Client*>::iterator it = --clients.end(); it != clients.begin(); --it)
	{
		if ((*it)->type == "server")
		{
			std::string srvrply[] = {
					"SERVER ft_irc_main 0 0 ",
					" no_info", "NULL" };
			sendmsg((*it)->socket, buildString(srvrply));
			return;
		}
	}
}

void	Server::links(Message &msg)
{
	int servers = 1;

	for (std::vector<Client*>::iterator it = this->clients.begin();
		it != this->clients.end(); ++it)
	{
		if ((*it)->type == "server")
			++servers;
	}
	std::string rpl[] = {":", ip, " ", RPL_LINKS, " ", msg.orig->nick, " "
	, "services. 127.0.0.1:", port, " ", ft_itoa(servers), " ft_irc", "NULL"};
	sendmsg(msg.orig->socket, buildString(rpl));
	for (std::vector<Client*>::iterator it = clients.begin(); it!= clients.end(); ++it)
	{
		if ((*it)->type == "server")
		{
			std::string rpl2[] = {
				":", ip, " ", RPL_LINKS, " ", msg.orig->nick, " services. ",
				(*it)->nick, " ", ft_itoa(servers), " [~ft_irc] "
				,"NULL"
			};
			sendmsg(msg.socket, buildString(rpl2));
		}
	}
	std::string endof[] = {":", ip, " ", RPL_ENDOFLINKS, " ", msg.orig->nick, \
		" * End of /LINKS list.", "NULL"};
	sendmsg(msg.socket, buildString(endof));
}

int		Server::exec(Message &msg)
{
	// 프로토콜에 맞추어 메시지를 처리하는 함수
	if (msg.command == "QUIT")
		quit(msg);
	else if (msg.command == "NOTICE")
	{
		// NOTICE 처리하는 부분
		notice(msg);
	}
	else if (msg.command == "SERVER")
		server(msg);
	else if (msg.command == "LEAKS")
	{
		// 메모리 릭 확인하기위하여 추가한 부분으로 추정됨
		system("leaks ircserv");
	}
	else if (msg.command == "PASS")
	{
		// 암호 입력을 처리하는 부분
		pass(msg);
	}
	else if (msg.command == "NICK")
	{
		// 사용자의 닉네임을 처리하는 부분
		nick(msg);
	}
	else if (msg.command == "USER")
	{
		// user 등록하는 부분
		user(msg);
	}
	else if (msg.orig->type == "unknown" || msg.orig->type == "accepted")
	{
		print("wrong permissions");
		print(msg.orig->type);
		return (-1);
	}
	else if (msg.command == "MODE")
		mode(msg);
	else if (msg.command == "OPER")
		oper(msg);
	else if (msg.command == "KICK")
		kick(msg);
	else if (msg.command == "LIST")
		list(msg);
	else if (msg.command == "TOPIC")
		topic(msg);
	else if (msg.command == "LUSERS")
		lusers(msg.orig);
	else if (msg.command == "JOIN")
		join(msg);
	else if (msg.command == "PRIVMSG")
		privmsg(msg);
	else if (msg.command == "PART")
		part(msg);
	else if (msg.command == "PING")
		ping(msg);
	else if (msg.command == "PONG")
		(void)msg;
	else if (msg.command == "MOTD")
		motd(msg.orig);
	else if (msg.command == "WHOIS")
		whois(msg);
	else if (msg.command == "WHO")
		who(msg);
	else if (msg.command == "NEWUSER")
		newuser(msg);
	else if (msg.command == "ADDCHANNEL")
		addchannel(msg);
	else if (msg.command == "NEWJOIN")
		newjoin(msg);
	else if (msg.command == "ADMIN")
		admin(msg);
	else if (msg.command == "INFO")
		info(msg);
	else if (msg.command == "VERSION")
		version(msg);
	else if (msg.command == "STATS")
		stats(msg);
	else if (msg.command == "TIME")
		time(msg);
	else if (msg.command == "TRACE")
		trace(msg);
	else if (msg.command == "CONNECT")
		connect(msg);
	else if (msg.command == "LINKS")
		links(msg);
	else if (msg.msg.length() != 2)
		unknown_command(msg);
	return (0);
}
