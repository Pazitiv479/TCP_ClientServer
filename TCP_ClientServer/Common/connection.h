#pragma once

#include "common.h"
#include "tsqueue.h"
#include "message.h"

template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>
{
public:
	// Соединение "принадлежит" либо серверу, либо клиенту, и его
	// поведение у них немного отличается.
	enum class owner
	{
		server,
		client
	};

	// Конструктор: Укажите владельца, подключитесь к контексту, передайте сокет
	//				Укажите ссылку на очередь входящих сообщений
	connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<ownedMessage<T>>& qIn)
		: asioContext(asioContext), socket(std::move(socket)), qMessagesIn(qIn)
	{
		nOwnerType = parent;
	}

	virtual ~connection()
	{
	}

	// Этот идентификатор используется во всей системе - так клиенты будут понимать других клиентов
			// существует во всей системе.
	uint32_t GetID() const
	{
		return id;
	}

	void ConnectToClient(uint32_t uid = 0)
	{
		if (nOwnerType == owner::server)
		{
			if (socket.is_open())
			{
				id = uid;
				ReadHeader();
			}
		}
	}

	void ConnectionToServer(const asio::ip::tcp::resolver::results_type& endpoints)
	{
		//Только клиенты могут подключаться к серверам
		if (nOwnerType == owner::client)
		{
			// Запросить попытку asio подключиться к конечной точке
			asio::async_connect(socket, endpoints,
				[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
				{
					if (!ec)
					{
						ReadHeader();
					}
				});
		}
	}
	
	void Disconnect()
	{
		if (IsConnected())
			asio::post(asioContext, [this]() { socket.close(); });
	}

	bool IsConnected() const 
	{
		return socket.is_open();
	}

	// ASYNC - отправка сообщения, соединения выполняются один к одному, поэтому указывать цель не нужно,
	// для клиента целью является сервер, и наоборот
	void Send(const messageBody<T>& msg)
	{
		asio::post(asioContext,
			[this, msg]()
			{
				// Если в очереди есть сообщение, то мы должны 
				// предположить, что оно находится в процессе асинхронной записи.
				// В любом случае добавьте сообщение в очередь для вывода. Если нет сообщений 
				// доступных для записи, запустите процесс записи
				// сообщения в начале очереди.
				bool bWritingMessage = !qMessagesOut.empty();
				qMessagesOut.push_back(msg);
				if (!bWritingMessage)
				{
					WriteHeader();
				}
			});
	}

private:
	// ASYNC - Основной контекст, готовый к чтению заголовка сообщения
	void ReadHeader()
	{
		// Если эта функция вызвана, мы ожидаем, что asio будет ждать, пока не получит
		// достаточное количество байт для формирования заголовка сообщения. Мы знаем, что заголовки имеют фиксированный размер,
		// поэтому выделите буфер передачи, достаточно большой для его хранения. Фактически,
		// мы создадим сообщение во "временном" объекте messageBdy, поскольку с ним 
		// удобно работать.
		asio::async_read(socket, asio::buffer(&msgTemporaryIn.header, sizeof(messageHeader<T>)),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// Заголовок сообщения прочитан полностью, проверьте, есть ли у этого сообщения текст, за которым можно следовать.
					if (msgTemporaryIn.header.size > 0)
					{
						// ...это так, поэтому выделите достаточно места в теле сообщения
						// vector и отправьте asio задание на чтение текста.
						msgTemporaryIn.body.resize(msgTemporaryIn.header.size);
						ReadBody();
					}
					else
					{
						// это не так, поэтому добавьте это сообщение без текста в
						// очередь входящих сообщений
						AddToIncomingMessageQueue();
					}
				}
				else
				{
					// При считывании данных с клиента произошла ошибка, скорее всего, произошло отключение
					// Закройте сокет и позвольте системе привести его в порядок позже.
					std::cout << "[" << id << "] Read Header Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - Prime context ready to read a message body
	void ReadBody()
	{
		// Если эта функция вызвана, заголовок уже прочитан, и этот заголовок
		// запрашивает, чтобы мы прочитали текст, пространство для этого текста уже выделено
		// во временном объекте сообщения, поэтому просто дождитесь поступления байтов...
		asio::async_read(socket, asio::buffer(msgTemporaryIn.body.data(), msgTemporaryIn.body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// ...и они это сделали! Теперь сообщение готово, поэтому добавьте
					// все сообщение целиком во входящую очередь
					AddToIncomingMessageQueue();
				}
				else
				{
					// Как указано выше!
					std::cout << "[" << id << "] Read Body Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - основной контекст для написания заголовка сообщения
	void WriteHeader()
	{
		// Если эта функция вызвана, мы знаем, что в очереди исходящих сообщений должно быть 
		// по крайней мере одно сообщение для отправки. Поэтому выделите буфер передачи для хранения
		// сообщения и выполните команду work - asio, отправив эти байты
		asio::async_write(socket, asio::buffer(&qMessagesOut.front().header, sizeof(messageHeader<T>)),
			[this](std::error_code ec, std::size_t length)
			{
				// asio уже отправил байты - если бы возникла проблема
				// было бы доступно сообщение об ошибке...
				if (!ec)
				{
					// ... ошибки нет, поэтому проверьте, есть ли в заголовке только что отправленного сообщения текст сообщения.
					if (qMessagesOut.front().body.size() > 0)
					{
						// ...это так, поэтому задайте задачу для записи байтов тела
						WriteBody();
					}
					else
					{
						// ...этого не произошло, поэтому мы закончили с этим сообщением. Удалите его из 
						// очереди исходящих сообщений
						qMessagesOut.pop_front();

						// Если очередь не пуста, нужно отправить еще несколько сообщений, поэтому
						// сделайте так, чтобы это произошло, выдав задание на отправку следующего заголовка.
						if (!qMessagesOut.empty())
						{
							WriteHeader();
						}
					}
				}
				else
				{
					// ...asio не удалось отправить сообщение, мы могли бы проанализировать причину, но 
					// пока просто предположим, что соединение прервалось из-за закрытия сокета. 
					// Когда в будущем попытка записи на этот клиент завершится неудачей из-за
					// закрытого сокета, это будет исправлено.
					std::cout << "[" << id << "] Write Header Fail.\n";
					socket.close();
				}
			});
	}

	// ASYNC - Основной контекст для написания текста сообщения
	void WriteBody()
	{
		// Если эта функция вызвана, только что был отправлен заголовок, и этот заголовок
		// указывает на то, что для этого сообщения существует основная часть. Заполните буфер передачи
		// данными основной части и отправьте его!
		asio::async_write(socket, asio::buffer(qMessagesOut.front().body.data(), qMessagesOut.front().body.size()),
			[this](std::error_code ec, std::size_t length)
			{
				if (!ec)
				{
					// Отправка прошла успешно, поэтому мы завершаем работу с сообщением
					// и удаляем его из очереди
					qMessagesOut.pop_front();

					// Если в очереди все еще есть сообщения, то выполните задание на 
					// отправку заголовка следующих сообщений.
					if (!qMessagesOut.empty())
					{
						WriteHeader();
					}
				}
				else
				{
					// Ошибка отправки, описание смотрите в эквиваленте функции WriteHeader()
					std::cout << "[" << id << "] Write Body Fail.\n";
					socket.close();
				}
			});
	}

	// Как только будет получено полное сообщение, добавьте его во входящую очередь
	void AddToIncomingMessageQueue()
	{
		// Помещаем его в очередь, преобразуя в "собственное сообщение", инициализируя
		// с помощью общего указателя из этого объекта connection
		if (nOwnerType == owner::server)
			qMessagesIn.push_back({ this->shared_from_this(), msgTemporaryIn });
		else
			qMessagesIn.push_back({ nullptr, msgTemporaryIn });

		// Теперь мы должны настроить контекст asio для получения следующего сообщения. Оно 
		// будет ждать, пока поступят байты, а затем будет сформировано сообщение
		// процесс повторяется.
		ReadHeader();
	}

protected:
	// Каждое соединение имеет уникальный сокет для подключения к удаленному компьютеру. 
	asio::ip::tcp::socket socket;

	// Этот контекст является общим для всего экземпляра asio
	asio::io_context& asioContext;

	// В этой очереди хранятся все сообщения, которые должны быть отправлены удаленной стороне
	// этого соединения
	tsqueue<messageBody<T>> qMessagesOut;

	// Cсылается на входящую очередь родительского объекта
	tsqueue<ownedMessage<T>>& qMessagesIn;

	// Входящие сообщения создаются асинхронно, поэтому мы будем
	// сохранять частично собранное сообщение здесь, пока оно не будет готово
	messageBody<T> msgTemporaryIn;

	// "Владелец" решает, как будут вести себя некоторые соединения
	owner nOwnerType = owner::server;
	uint32_t id = 0;
};
