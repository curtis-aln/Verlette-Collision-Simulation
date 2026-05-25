#pragma once

#include <SFML/Graphics.hpp>


struct Circle
{
	std::vector<unsigned int> m_circleIndexes;

	explicit Circle(std::vector<unsigned int> circleIndexes) : m_circleIndexes(std::move(circleIndexes)) {}
	~Circle() = default;

	sf::Vector2f getCenter(sf::VertexArray& circles) const;

	void move(sf::VertexArray& circles, sf::Vector2f deltaPosition) const;
	void setPosition(sf::VertexArray& circles, sf::Vector2f position) const;
	void setColor(sf::VertexArray& circles, const sf::Color& color) const;
	sf::Color getColor(sf::VertexArray& circles) const;
};



class ArrayOfCircles
{
public:
	std::vector<Circle> m_circles{};
	sf::VertexArray m_circleArray{};


private:
	unsigned int pointsInCircle{};
	unsigned int m_circleCount{};
	unsigned int totalCircleVertexCount{};
	float m_circleRadius{};

	float PI = 3.141592f;


public:
	explicit ArrayOfCircles(unsigned int circleCount = 0, float circleRadius = 0, unsigned int circlePoints = 0);
	~ArrayOfCircles() = default;

private:
	void fillArrWithArr();
	void convertVertexArrayToCircles(const sf::VertexArray& vertexArray);

	std::vector<sf::Vector2f> createTriangleVertecies() const;
	sf::Vector2f idxToCoords(unsigned int idx) const;


	
};