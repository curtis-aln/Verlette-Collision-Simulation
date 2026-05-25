#pragma once

#include <SFML/Graphics.hpp>

#include "circles/circles.hpp"

class Entity
{
	sf::Vector2f m_velocity{};

	sf::Color m_colorActive{};
	sf::Color m_colorInactive{};

	const float  m_radius{};
	const float  m_radiusSquared{};
	const float  m_maxSpeed{};

	const sf::Rect<float>& m_border{};

public:
	sf::Vector2f p_position{};
	std::vector<Entity*> p_nearby{};

	const unsigned int id{};


	// constructor and destructor
	explicit Entity(const sf::Vector2f position = {0, 0}, const sf::Vector2f velocity = { 0, 0 }, const sf::Color colorActive = { 0, 0, 0 }, 
	                const sf::Color colorInactive = { 0, 0, 0 }, const float interactionRadius=1, const unsigned int _id=1, 
	                const float maxSpeed = 1, const sf::Rect<float>& border = { {0, 0}, {0, 0} })
		: m_velocity(velocity), m_colorActive(colorActive), m_colorInactive(colorInactive), m_radius(interactionRadius),
		m_radiusSquared(m_radius * m_radius), m_maxSpeed(maxSpeed), m_border(border), p_position(position), id(_id) {}

	~Entity() = default;


	void update(ArrayOfCircles& allCircles)
	{
		const Circle& renderCircle = allCircles.m_circles[id];

		const sf::Color currentColor = renderCircle.getColor(allCircles.m_circleArray);
		interactWithNearby(renderCircle, allCircles, currentColor);

		speed_limit();
		updatePosition(allCircles, renderCircle); // slow
		borderCollision();
	}

	sf::Vector2f getPosition() const
	{
		return p_position;
	}


private:
	void borderCollision()
	{
		const float buffer = m_radius;

		const bool x_out_of_bounds = p_position.x < m_border.position.x + buffer || p_position.x > m_border.position.x + m_border.size.x - buffer;
		const bool y_out_of_bounds = p_position.y < m_border.position.y + buffer || p_position.y > m_border.position.y + m_border.size.y - buffer;

		if (x_out_of_bounds) {
			m_velocity.x *= -1;
		}

		if (y_out_of_bounds) {
			m_velocity.y *= -1;
		}

		p_position.x = std::max(m_border.position.x + buffer, std::min(p_position.x, m_border.position.x + m_border.size.x - buffer));
		p_position.y = std::max(m_border.position.y + buffer, std::min(p_position.y, m_border.position.y + m_border.size.y - buffer));
	}


	void updatePosition(ArrayOfCircles& allCircles, const Circle& renderCircle)
	{
		p_position += m_velocity;
		renderCircle.setPosition(allCircles.m_circleArray, p_position);
	}


	void speed_limit()
	{
		if (const float speed = sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y); speed > m_maxSpeed) 
		{
			m_velocity.x = (m_velocity.x / speed) * m_maxSpeed;
			m_velocity.y = (m_velocity.y / speed) * m_maxSpeed;
		}
	}


	bool checkNearbyCollision() const
	{
	    for (const Entity* entity : p_nearby)
	    {
	        const float dx = entity->p_position.x - p_position.x;
	        const float dy = entity->p_position.y - p_position.y;
	        const float distSquared = dx * dx + dy * dy;

	        if (distSquared <= m_radiusSquared*2 && entity->id != id)
	            return true;
	    }
	    return false;
	}



	void interactWithNearby(const Circle& renderCircle, ArrayOfCircles& allCircles, const sf::Color& currentColor) const
	{
		if (checkNearbyCollision())
		{
			if (currentColor != m_colorActive)
			{
				renderCircle.setColor(allCircles.m_circleArray, m_colorActive);
			}
		}
		else
		{
			if (currentColor != m_colorInactive)
			{
				renderCircle.setColor(allCircles.m_circleArray, m_colorInactive);
			}
		}
	}

};