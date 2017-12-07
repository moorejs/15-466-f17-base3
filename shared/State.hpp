#pragma once

#include <array>

#include "Collisions.h"

namespace Data {

static Collision collisionFramework = []() {
	Collision framework = Collision({{-1.92f, -7.107f}, {6.348f, 9.775f}});

	// TODO: asset pipeline bounding boxes. Hard coded for now.
	// these are the four houses
	framework.addBounds({{0.719, -1.003}, {2.763, 2.991}});
	framework.addBounds({{4.753f, -2.318f}, {6.737f, -0.24f}});
	framework.addBounds({{4.451f, 6.721f}, {9.929f, 8.799f}});
	framework.addBounds({{0.247f, 7.31f}, {2.397f, 9.986f}});

	return framework;
}();

static std::array<BBox2D, 1> roadblocks = {{BBox2D({0.0f, 0.0f}, {1.0f, 1.0f})}};

};	// namespace Data

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
	ACTION,
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