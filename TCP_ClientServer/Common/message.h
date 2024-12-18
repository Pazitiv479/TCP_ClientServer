#pragma once

#include "common.h"


//Структура заголовка сообщения
template <typename T>
struct messageHeader
{
	T id{};
	uint32_t size = 0;
};

//Структура тела сообщения
template <typename T>
struct messageBody
{
	messageHeader<T> header;
	std::vector<uint8_t> body; //uint8_t - для работы с байтами, std::vector хранит необработанные байты данных

	// Возвращает размер всего пакета сообщения в байтах
	size_t size() const
	{
		return sizeof(messageHeader<T>) + body.size();
	}

	// Переопределение для совместимости с std::cout - выводит удобочитаемое описание сообщения
	friend std::ostream& operator << (std::ostream& os, const messageBody<T>& msg)
	{
		os << "ID: " << int(msg.header.id) << " Size: " << msg.header.size;
	}

	// Добавляет любые простые старые (привычные) типы данных данные в буфер сообщения.
	// POD (Plain Old Data) - Простой старый (или привычный) тип данных
	template<typename DataType>
	friend messageBody<T>& operator << (messageBody<T>& msg, const DataType& data)
	{
		// Проверяет, что тип добавляемых данных тривиально копируемый.
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

		// Сохраняет текущий размер вектора, так как это будет точка, в которую мы вставим данные.
		size_t i = msg.body.size();

		// Изменяет размер вектора на размер добавляемых данных.
		msg.body.resize(msg.body.size() + sizeof(DataType));

		// Физически скопирует данные в недавно выделенное пространство вектора.
		// + i место с которого добавляются данные
		std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

		// Обновление переменной, объявленной в зщаголовке сообщения
		msg.header.size = msg.size();

		return msg;
	}

	// Извлекает любые простые старые (привычные) типы данных данные в буфер сообщения.
	// POD (Plain Old Data) - Простой старый (или привычный) тип данных
	template<typename DataType>
	friend messageBody<T>& operator >> (messageBody<T>& msg, DataType& data)
	{
		// Проверка, что тип извлекаемых данных тривиально копируемый
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

		// Вычитание размера извлекаемых данных с конца вектора
		size_t i = msg.body.size() - sizeof(DataType);

		// Физическое копирвание данных из вектора в переменную пользователя
		std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

		// Уменьшение размера вектора, чтобы удалить прочитанные байты, и сброс конечной позиции
		msg.body.resize(i);

		// Обновление переменной, объявленной в зщаголовке сообщения
		msg.header.size = msg.size();

		return msg;
	}
};

// "ownedMessage" сообщение идентично обычному сообщению, но оно связано ссоединением. 
// На сервере владельцем будет клиент, отправивший сообщение, 
// на клиенте владельцем будет сервер.

// Предварительное объявление класса соединения
template <typename T>
class connection;


// Структура, котрая покажет серверу от какого клиента пришло сообщение
template <typename T> 
struct ownedMessage
{
	std::shared_ptr<connection<T>> remote = nullptr;
	messageBody<T> msg;

	// Функция для вывода в поток при  помощи cout
	friend std::ostream& operator << (std::ostream& os, const ownedMessage<T>& msg)
	{
		os << msg.msg;
		return os;
	}
};


