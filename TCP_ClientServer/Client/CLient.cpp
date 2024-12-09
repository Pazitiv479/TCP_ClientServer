#include <iostream>
#include <net.h>

enum class CustomMsgTypes : uint32_t //����� �������������� ����� �������� �� 4 ������
{
	FireBullet,
	MovePlayer
};

class CustomClien : public ClientInterface<CustomMsgTypes>
{
public:
	bool FIreBullet(float x, float y)
	{
		messageBody<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::FireBullet;
		msg << x << y;
		//Send(msg);
	}
};

int main()
{
	messageBody<CustomMsgTypes> msg;
	msg.header.id = CustomMsgTypes::FireBullet;

	int a = 1;
	bool b = true;
	float c = 3.14159f;

	struct 
	{
		float x;
		float y;
	} d[5];

	msg << a << b << c << d;

	a = 99;
	b = false;
	c = 99.09f;

	msg >> d >> c >> b >> a;

	return  0;
} 