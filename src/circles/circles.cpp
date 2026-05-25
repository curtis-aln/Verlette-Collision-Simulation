#include "circles.hpp"
#include <numeric>


void Circle::move(sf::VertexArray& circles, const sf::Vector2f deltaPosition) const
{
	for (const unsigned int& index : m_circleIndexes)
	{
		circles[index].position.x += deltaPosition.x;
		circles[index].position.y += deltaPosition.y;
	}
}


sf::Vector2f Circle::getCenter(sf::VertexArray& circles) const
{
	float sumsX = 0;
	float sumsY = 0;

	for (const unsigned int index : m_circleIndexes)
	{
		sumsX += circles[index].position.x;
		sumsY += circles[index].position.y;
	}

	const auto count = static_cast<float>(m_circleIndexes.size());
	return { sumsX / count, sumsY / count };
}


void Circle::setPosition(sf::VertexArray& circles, const sf::Vector2f position) const
{
	const sf::Vector2f centerPos = getCenter(circles);
	move(circles, { position.x - centerPos.x, position.y - centerPos.y });
}


void Circle::setColor(sf::VertexArray& circles, const sf::Color& color) const
{
	for (const unsigned int& index : m_circleIndexes)
	{
		circles[index].color = color;
	}
}

sf::Color Circle::getColor(sf::VertexArray& circles) const
{
	return circles[m_circleIndexes[0]].color;
}


ArrayOfCircles::ArrayOfCircles(const unsigned int circleCount, const float circleRadius, const unsigned int circlePoints) :
	pointsInCircle(circlePoints), m_circleCount(circleCount), m_circleRadius(circleRadius)
{
	// checking if this is a defualt initilisation
	if (circleCount == 0 || circleRadius == 0 || circlePoints == 0)
		return;

	// preparing the vertex array
	totalCircleVertexCount = (pointsInCircle * 3) * m_circleCount;
	m_circleArray.setPrimitiveType(sf::PrimitiveType::Triangles);
	m_circleArray.resize(totalCircleVertexCount);

	// preparing the circle classes
	m_circles.reserve((pointsInCircle));

	// filling the data
	fillArrWithArr();
	convertVertexArrayToCircles(m_circleArray);

}

void ArrayOfCircles::fillArrWithArr() {
	const std::vector<sf::Vector2f> triangleVertecies = createTriangleVertecies();

	// Keep track of the current index in arr1
	size_t currentIndex = 0;

	// Iterate through arr2
	for (size_t i = 0; i < m_circleArray.getVertexCount(); i++)
	{
		// Fill arr2 with the elements of arr1
		m_circleArray[i].position = triangleVertecies[currentIndex];

		// Increase the current index in arr1
		currentIndex++;

		// If we have reached the end of arr1, start again from the beginning
		if (currentIndex >= triangleVertecies.size())
		{
			currentIndex = 0;
		}
	}
}



std::vector<sf::Vector2f> ArrayOfCircles::createTriangleVertecies() const
{
	std::vector<sf::Vector2f> triangles;
	triangles.reserve(static_cast<int>(pointsInCircle * 3));

	for (unsigned int i = 0; i < pointsInCircle; i++)
	{
		triangles.emplace_back(idxToCoords(i));    // vertex 1
		triangles.emplace_back(idxToCoords(i + 1));// vertex 2
		triangles.emplace_back(0, 0);         // vertex center
	}
	return triangles;
}


sf::Vector2f ArrayOfCircles::idxToCoords(const unsigned int idx) const
{
	const float x = cos(2 * PI / static_cast<float>(pointsInCircle) * static_cast<float>(idx)) * m_circleRadius;
	const float y = sin(2 * PI / static_cast<float>(pointsInCircle) * static_cast<float>(idx)) * m_circleRadius;
	return { x, y };
}


void ArrayOfCircles::convertVertexArrayToCircles(const sf::VertexArray& vertexArray)
{
	const unsigned int pointsPerCircle = totalCircleVertexCount / m_circleCount;

	for (unsigned int i = 0; i < vertexArray.getVertexCount(); i += pointsPerCircle)
	{
		std::vector<unsigned int> points(pointsPerCircle);
		std::iota(points.begin(), points.end(), i);

		m_circles.emplace_back(points);
	}
}
