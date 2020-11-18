#include "../inc/Message.hpp"

Message::Message(std::string const &msg, Client *orig)
	:msg(msg), socket(orig->getSocket()), orig(orig)
{
	if (msg.length() < 3)
	{
		// command의 길이가 최소한 3자리 이상이기 때문에 3보다 작은 값은 에러로 처리
		command = "NULL";
		return;
	}
	split(msg.substr(0, msg.length() - 2), params, ' ');
	// 메시지에서 prefix command parameter의 구분은 공백 ' '으로 이루어짐
	// => 메시지의 맨 마지막 두 문자는 cr_lp이므로
	// msg.length() - 2를 넣은 이유는 irc프로토콜에서 메시지는 /r/n으로 끝나기 때문
	// 하지만, nc명령어를 이용할 때에는 /r을 넣어주기가 쉽지 않음
	// 이경우 문자열의 맨 마지막이 짤리게 됨
	if (params.begin()->at(0) == ':')
	{
		// prefix가 존재하는경우 prefix설정함
		prefix = *params.begin();
		params.pop_front();
	}
	command = *params.begin();
	// command값 설정함
	if (params.size())
		params.pop_front();
	// params에서 command를 제거
}

Message::~Message(){}

void Message::printMsg(void)
{
	// 만들어놓고 쓰지 않음
	std::cout << "PR:" <<this->prefix << std::endl;
	std::cout << "CO:" <<this->command << std::endl;
	print_list(this->params);
}
