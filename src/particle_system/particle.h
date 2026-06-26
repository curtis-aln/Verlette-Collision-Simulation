#pragma once

#include <SFML/Graphics.hpp>
#include "settings.h"

struct Entity
{
public:
	uint32_t id_{};

	sf::Vector2f position_{};
	sf::Vector2f velocity_{};
	sf::Color color_{};
	float radius_{};

	// constructor and destructor
	explicit Entity(const sf::Vector2f position = {0, 0}, const sf::Vector2f velocity = { 0, 0 }, const uint32_t _id=1)
		: velocity_(velocity), position_(position), id_(_id) {	}

	~Entity() = default;


	void update()
	{
		updatePosition();
	}


private:
	void updatePosition()
	{
		position_ += velocity_;
	}
};