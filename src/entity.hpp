#pragma once

#include <SFML/Graphics.hpp>

class Entity
{
public:
	unsigned int id_{};

	sf::Vector2f position_{};
	sf::Vector2f velocity_{};

	sf::Color m_colorActive{};
	sf::Color m_colorInactive{};

	const float  m_radius{};
	const float  m_radiusSquared{};
	const float  m_maxSpeed{};

	const sf::Rect<float>& m_border{};

	bool collided = false;

	
	// constructor and destructor
	explicit Entity(const sf::Vector2f position = {0, 0}, const sf::Vector2f velocity = { 0, 0 }, const sf::Color colorActive = { 0, 0, 0 }, 
	                const sf::Color colorInactive = { 0, 0, 0 }, const float interactionRadius=1, const unsigned int _id=1, 
	                const float maxSpeed = 1, const sf::Rect<float>& border = { {0, 0}, {0, 0} })
		: velocity_(velocity), m_colorActive(colorActive), m_colorInactive(colorInactive), m_radius(interactionRadius),
		m_radiusSquared(m_radius * m_radius), m_maxSpeed(maxSpeed), m_border(border), position_(position), id_(_id) 
	{	
	}

	~Entity() = default;


	void update()
	{

		speed_limit();
		updatePosition();
		borderCollision();
	}

	sf::Vector2f getPosition() const
	{
		return position_;
	}


private:
	void borderCollision()
	{
		const float buffer = m_radius;

		const bool x_out_of_bounds = position_.x < m_border.position.x + buffer || position_.x > m_border.position.x + m_border.size.x - buffer;
		const bool y_out_of_bounds = position_.y < m_border.position.y + buffer || position_.y > m_border.position.y + m_border.size.y - buffer;

		if (x_out_of_bounds) {
			velocity_.x *= -1;
		}

		if (y_out_of_bounds) {
			velocity_.y *= -1;
		}

		position_.x = std::max(m_border.position.x + buffer, std::min(position_.x, m_border.position.x + m_border.size.x - buffer));
		position_.y = std::max(m_border.position.y + buffer, std::min(position_.y, m_border.position.y + m_border.size.y - buffer));
	}


	void updatePosition()
	{
		position_ += velocity_;
	}


	void speed_limit()
	{
		if (const float speed = sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y); speed > m_maxSpeed) 
		{
			velocity_.x = (velocity_.x / speed) * m_maxSpeed;
			velocity_.y = (velocity_.y / speed) * m_maxSpeed;
		}
	}
};