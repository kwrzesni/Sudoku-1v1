#include "Room.h"

int Room::newRoomId = 1;

void Room::ChangeGuestToHost()
{
	host = guest;
	guest.name = "";
}
