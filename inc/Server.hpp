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
		// "HELLO_MADRID"이 저장되고, 사용도 변경되 안됨
		std::string creationtime;
		// "02/09/2019 09:00:00 GMT+1"이 값이 저장되고 변경되지 않음 한번 사용됨
		std::string ip;

		std::vector<Client*> clients;
		std::vector<std::string> otherclients;
		std::vector<Channel> channels;
		std::vector<Channel> otherchannels;

		SSL_CTX	*ctx;
		SSL *ssl;

		// 파일디스크립터의 인풋에관한 그룹 관련 변수
		fd_set	master;
		// 사실상 쓸모없는 값임
		fd_set	read_fds;
		// 외부와의 연결을 담당하는 파일디스크립터
		int		listener;
		int		fdmax;
		std::vector<std::string> commands;

		Server();
		Server(Server const &cpy);
		Server &operator=(Server const &cpy);

	public:
		Server(int argc, char **argv);
		// 서버의 인자 개수 처리 및 패스워드 및 포트 저장
		virtual ~Server();

		void clear();

		void serv_connect();
		void server_login();
		void init_server();
		// 외부의 접속을 관리하는 파일디스크립터 설정하는 함수
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
		// 메시지 전송하는 함수
		void receive(int socket);
		// 소켓으로 부터 전송되는 메시지 수신 및 처리
		void ssl_receive(int socket);
		void receive_noexec(int socket);

		Client* getClient(int socket);
		// socket에 해당하는 클라이언트를 받아옴
		std::string getOtherClient(std::string const &nick);
		// 서버에 직접 연결되어있지 않은 클라이언트에 대한 클라이언트 정보를 받아오는 함수
		Client* getClientByUser(std::string const &user);
		Client* getClient(std::string const &nick);
		Channel &getChannel(std::string const &ch);
		// 해당하는 채널을 받아옴
		Channel &getOtherChannel(std::string const &ch);
		void deleteClient(int socket);
		// 소켓에 해당하는 client를 삭제함
		void sendjoin(const std::string &name, const std::string &nick);
		void quitother(std::string const &nick);
		// otherClient에서 클라이언트 삭제
		void partother(std::string const &nick);
		// otherChannels에서 클라이언트 삭제

		bool isvalid(Message &msg);
		int exec(Message &msg);
		// 프로토콜에 맞추어 메시지를 처리하는 부분
		bool isServer(std::string const &nick);

		std::vector<Channel>::iterator exists(std::string &channel);
		// 해당하는 채널이 있는지 체크하는 함수
		// 찾은경우 해당하는 이터레이터 리턴
		// 찾지 못한경우 end()리턴

		void quit(Message &msg);
		// 클라이언트와의 연결을 종료하는 함수
		void pass(Message &msg);
		// pass메시지 처리하는 함수
		void nick(Message &msg);
		// nick을 등록하려는 함수
		// TODO server preserver nick설정 확인하지 않음
		void user(Message &msg);
		// user등록하는 함수
		void privmsg(Message &msg);
		// PRIVMSG처리하는 함수
		void join(Message &msg);
		void part(Message &msg);
		void lusers(Client *cli);
		// 현재 서버와 연결하고 있는 클라이언트의 상태를 알려줌
		void server(Message &msg);
		// server메시지 처리하는 함수
		void kick(Message &msg);
		void ping(Message &msg);
		void pong(Message &msg);
		void list(Message &msg);
		void motd(Client *cli);
		// motd(Message Of The Day)메시지 처리
		void oper(Message &msg);
		void topic(Message &msg);
		void notice(Message &msg);
		// notice메시지 처리하는 함수
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
		// 최초로 닉네임을 등록하는 경우
		void re_nick(Message &msg);
		// 닉네임 재설정
		void sendusers(Message &msg);
		void sendchannels(Message &msg);
		void newjoin(Message &msg);

		void forward(std::string const &str);
		// server와 연결된 클라이언트중 server또는 preserver인 클라이언트에 모두 메시지를 보냄
		void broadcast(std::string const &orig, std::string const &rpl, std::string const &msg);
		void tochannel(std::string const& channel, std::string const &msg, int orig);
		// 채널에 참석하고있는 모두에게 메시지 전송
		void tochannel(Channel &ch, Message &msg);
		void not_params(Message &msg);
		void no_such_channel(Message &msg);
		void no_such_server(Message &msg);
		void not_in_channel(Message &msg);
		void not_operator(Message &msg);
		void no_such_nick_channel(Message &msg);
		void unknown_command(Message &msg);
		void welcome(const Client &cli);
		// 유저 등록이 끝난경우 처리 / 연결된 다른 서버에 전송 /
};

# endif
