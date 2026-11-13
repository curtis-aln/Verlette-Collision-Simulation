#pragma once

#include <SFML/Graphics.hpp>
#include "settings.h"
#include "body.h"

struct Entity : public Body
{
	sf::Color color_rest_{};
	sf::Color color_max_{};

	sf::Color color_{};

	// constructor and destructor
	explicit Entity(const Body& body = {}) : Body(body) {}

	~Entity() = default;
};