#ifndef MESSAGE_HPP
# define MESSAGE_HPP
# include "ft_irc.hpp"

class Client;

class Message
{
	// 클라이언트의 메시지를 파싱받아 저장하는 클래스

	// irc 프로토콜에서의 메시지 구성
	// => "prefix command parameter"
	// 각각의 prefix, command, parameter의 구분은 ASCII space character(' ', 0x20)으로 구분됨

	// prefix : 항상 존재하지 않음 / prefix가 있는지 여부 파악은 ASCII colon character (':', 0x3b)
	// => prefix가 있는경우 메시지 맨 앞에 콜론이 와야함
	// => 콜론과 prefix사이에는 whitespace가 존재할 수 없음
	// => prefix는 서버간 통신에서 메시지의 실제 위치를 가리키기위해 사용됨
	// => prefix가 생략되어 있는경우 메시지를 받는 쪽과의 연결이 성립되었다고 가정
	// => 클라이언트는 메시지에서 prefix를 사용해서는 안됨
	// => 클라이언트에서 사용이 가능한 prefix는 클라이언트의 닉네임 설정뿐임
	// RFC 2812 p4 ~ p5 2.3 messages

	// command : irc command 또는 3자리의 아스키 코드값 만 가능

	// messages : CR_LF이전까지의 문자열의미함 / CR_LF까지 합쳐서 512를 넘어가서는 안됨
	// => CR_LF을 제외하고 문자열의 길이는 510이 max
	// CR_LF : \r\n의미 src/Server.cpp sendmsg함수 참고
	// there is no provision for continuation of message lines. = 멀티라인 처리를 할 수 없음으로 이해함

	// message formet
	// cr_lf를 이용하여 메시지를 구분함

	friend class Server;

	private:
		std::string msg;
		std::string prefix;
		std::string command;
		int nparams;
		std::list<std::string> params;
		int socket;
		Client *orig;

		Message();
		Message(const Message &);
		Message &operator=(const Message &);

	public:
		Message(std::string const &msg, Client *orig);
		~Message();

		void printMsg(void);
};

#endif
