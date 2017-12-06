#pragma once

#include <array>

#include "Collisions.h"

namespace Data {
static std::array<BBox2D, 1> roadblocks = {{BBox2D({0.0f, 0.0f}, {1.0f, 1.0f})}};
};

enum Power : uint8_t {
	NONE,
	SNAPSHOT,
	ANON_TIP,
	ROADBLOCK,
	DEPLOY,
};

enum Control : uint8_t {
	UP,
	DOWN,
	LEFT,
	RIGHT,
};

enum MessageType : uint8_t {
	STAGING_PLAYER_CONNECT,
	STAGING_PLAYER_DISCONNECT,
	STAGING_VOTE_TO_START,
	STAGING_VETO_START,
	STAGING_START_GAME,
	STAGING_ROLE_CHANGE,
	STAGING_ROLE_CHANGE_REJECTION,
	STAGING_PLAYER_SYNC,
	INPUT,
	GAME_ROBBER_POS,
	GAME_COP_POS,
	GAME_ACTIVATE_POWER,
	GAME_TIME_OVER,
};

struct Packet {
	uint8_t header;
	std::vector<uint8_t> payload;

	static Packet* pack(MessageType type, std::initializer_list<uint8_t> extra = {}) {
		Packet* packet = new Packet();

		packet->payload.emplace_back(type);
		packet->payload.insert(packet->payload.end(), extra.begin(), extra.end());

		packet->header = packet->payload.size();

		return packet;
	}

	static Packet* pack(MessageType type, std::vector<uint8_t> extra) {
		Packet* packet = new Packet();

		packet->payload.emplace_back(type);
		packet->payload.insert(packet->payload.end(), extra.begin(), extra.end());

		packet->header = packet->payload.size();

		return packet;
	}
};

struct SimpleMessage {
	uint8_t id;

	static const SimpleMessage* unpack(Packet* packet) {
		return reinterpret_cast<const SimpleMessage*>(packet->payload.data() + 1);
	}
};