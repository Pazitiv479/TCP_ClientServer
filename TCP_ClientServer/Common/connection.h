#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"

template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>
{
public:
	connectiob()
	{
	}

	virtual ~connection()
	{
	}

	bool ConnectionToServer();
	bool Disconnectrd();
	bool IsConnected() const;

	bool Send(const messageBody<T>& msg);

protected:
	// Каждое соединение имеет уникальный сокет для подключения к удаленному компьютеру. 
	asio::ip::tcp::socket socket;

	// Этот контекст является общим для всего экземпляра asio
	asio::io_context& asioContext;

	// В этой очереди хранятся все сообщения, которые должны быть отправлены удаленной стороне
	// этого соединения
	tsqueue<message<T>> qMessagesOut;

	// Cсылается на входящую очередь родительского объекта
	tsqueue<owned_message<T>>& qMessagesIn;
};
