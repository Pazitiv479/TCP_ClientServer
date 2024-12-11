#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"
#include "connection.h"

template <typename T>
class ClientInterface
{
public:
	ClientInterface()
	{
	}

	virtual ~ClientInterface()
	{
		// If the client is destroyed, always try and disconnect from server
		Disconnect();
	}

	// Подключиться к серверу, указав имя хоста/ip-адрес и порт
	bool Connect(const std::string& host, const uint16_t port)
	{
		try
		{
			connection = std::make_unique<Connection<T>>();

			// Преобразовать имя хоста/ip-адрес в реальный физический адрес
			asio::ip::tcp::resolver resolver(context);
			asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
			//asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

			// Укажите объекту подключения, что он должен подключиться к серверу
			connection->ConnectToServer(endpoints);

			//// Создание подключения
			//connection = std::make_unique<connection<T>>(connection<T>::owner::client, context, asio::ip::tcp::socket(context), qMessagesIn);

			// Запустить контекстный поток
			thrContext = std::thread([this]() { context.run(); });
		}
		catch (std::exception& e)
		{
			std::cerr << "Client Exception: " << e.what() << "\n";
			return false;
		}

		return false;
	}

	// Отключиться от сервера
	void Disconnect()
	{
		// Если соединение существует, и тогда оно подключено...
		if (IsConnected())
		{
			// ...корректно отключиться от сервера
			connection->Disconnect();
		}

		// В любом случае, мы также закончили с контекстом asio...				
		context.stop();
		// ...и его нить
		if (thrContext.joinable())
			thrContext.join();

		// Уничтожить объект подключения
		connection.release();
	}

	// Проверка подключения к серверу
	bool IsConnected()
	{
		if (connection)
			return connection->IsConnected();
		else
			return false;
	}

	// Получить очередь сообщений с сервера
	tsqueue<ownedMessage<T>>& Incoming()
	{
		return qMessagesIn;
	}

protected:
	// контекст asio обрабатывает передачу данных...
	asio::io_context context;
	// ...но нуждается в собственном потоке для выполнения своих рабочих команд
	std::thread thrContext;
	// У клиента есть единственный экземпляр объекта "connection", который обрабатывает передачу данных
	std::unique_ptr<connection<T>> connection;

private:
	// Потокобезопасная очередь входящих сообщений с сервера
	tsqueue<ownedMessage<T>> qMessagesIn;
};
