#ifndef SERVER_HPP
# define SERVER_HPP
# include "ft_irc.hpp"

class Client;
class Message;
class Channel;

class Server
{

	private:
		std::string host;
		std::string port_network;
		std::string password_network;
		std::string port;
		std::string password;
		std::string msgofday;
		std::string creationtime;
		std::string ip;

		std::vector<Client*> clients;
		std::vector<std::string> otherclients;
		std::vector<Channel> channels;
		std::vector<Channel> otherchannels;

		SSL_CTX	*ctx;
		SSL *ssl;

		fd_set	master;
		fd_set	read_fds;
		int		listener;
		int		fdmax;
		std::vector<std::string> commands;

		Server();
		Server(Server const &cpy);
		Server &operator=(Server const &cpy);

	public:
		Server(int argc, char **argv);
		virtual ~Server();

		void clear();

		void serv_connect();
		void server_login();
		void init_server();
		void ssl_init();
		void main_loop();
		// 서버와의 통신을 수신하는 함수
		void serv_select();
		// select를 이용하여 그룹화된 fd에 변화가 생겼는지 체크
		void ssl_serv_select();
		void new_connection();
		// 클라이언트의 접속 요청을 처리하는 함수
		void ssl_new_connection();
		std::string getInfo(void) const;
		std::string getIp(void);

		void sendmsg(int socket, std::string const &str);
		// secket에 해당하는 클라이언트에 메시지 전송
		void sendmsg(int orig, int dest, std::string code, std::string const &str);
		void receive(int socket);
		// 소켓으로 부터 전송되는 메시지 수신 및 처리
		void ssl_receive(int socket);
		void receive_noexec(int socket);

		Client* getClient(int socket);
		// socket에 해당하는 클라이언트를 받아옴
		std::string getOtherClient(std::string const &nick);
		Client* getClientByUser(std::string const &user);
		Client* getClient(std::string const &nick);
		Channel &getChannel(std::string const &ch);
		Channel &getOtherChannel(std::string const &ch);
		void deleteClient(int socket);
		// 소켓에 해당하는 client를 삭제함
		void sendjoin(const std::string &name, const std::string &nick);
		void quitother(std::string const &nick);
		void partother(std::string const &nick);

		bool isvalid(Message &msg);
		int exec(Message &msg);
		bool isServer(std::string const &nick);

		std::vector<Channel>::iterator exists(std::string &channel);

		void quit(Message &msg);
		void pass(Message &msg);
		void nick(Message &msg);
		void user(Message &msg);
		void privmsg(Message &msg);
		void join(Message &msg);
		void part(Message &msg);
		void lusers(Client *cli);
		void server(Message &msg);
		void kick(Message &msg);
		void ping(Message &msg);
		void pong(Message &msg);
		void list(Message &msg);
		void motd(Client *cli);
		void oper(Message &msg);
		void topic(Message &msg);
		void notice(Message &msg);
		void whois(Message &msg);
		void who(Message &msg);
		void mode(Message &msg);
		void version(Message &msg);
		void stats(Message &msg);
		void links(Message &msg);
		void time(Message &msg);
		void connect(Message &msg);
		void trace(Message &msg);
		void admin(Message &msg);
		void info(Message &msg);

		void newuser(Message &msg);
		void addchannel(Message &msg);

		void new_nick(Message &msg);
		void re_nick(Message &msg);
		void sendusers(Message &msg);
		void sendchannels(Message &msg);
		void newjoin(Message &msg);

		void forward(std::string const &str);
		void broadcast(std::string const &orig, std::string const &rpl, std::string const &msg);
		void tochannel(std::string const& channel, std::string const &msg, int orig);
		void tochannel(Channel &ch, Message &msg);
		void not_params(Message &msg);
		void no_such_channel(Message &msg);
		void no_such_server(Message &msg);
		void not_in_channel(Message &msg);
		void not_operator(Message &msg);
		void no_such_nick_channel(Message &msg);
		void unknown_command(Message &msg);
		void welcome(const Client &cli);
};

# endif
