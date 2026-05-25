#pragma once

struct ZoomableVertexArray {
    sf::VertexArray m_vertexArray{};
    sf::Transform m_transform{};
    float m_zoomStrength{};

    const float m_screenWidth{};
    const float m_screenHeight{};

    ZoomableVertexArray(const sf::VertexArray* vertexArray, const float zoomStrength, const float screenWidth, const float screenHeight)
        : m_vertexArray(*vertexArray), m_zoomStrength(zoomStrength), m_screenWidth(screenWidth), m_screenHeight(screenHeight) {}


    void update(const float mouseWheelScroll_delta, const sf::Vector2f& offset)
    {
        // apply the translation offset
        m_transform.translate(offset);

        // update the transform
        if (mouseWheelScroll_delta > 0)
        {
            // mouse wheel scrolled up, zoom in
            m_transform.scale({1.0f + m_zoomStrength, 1.0f + m_zoomStrength});
        }
        else
        {
            // mouse wheel scrolled down, zoom out
            m_transform.scale({1.0f - m_zoomStrength, 1.0f - m_zoomStrength});
        }

        // apply the opposite of the translation offset
        m_transform.translate(-offset);
    }

    void translate(const sf::Vector2f deltaPosition)
    {
        const sf::Vector2f offSet = deltaPosition;
        m_transform.translate({offSet.x, offSet.y});
    }

    void updateMousePosition(const sf::Vector2f& mousePos, const float mouseWheelScroll_delta) {
        const auto offset = m_transform.getInverse().transformPoint(mousePos);
        update(mouseWheelScroll_delta, offset);
    }

    void drawVertexArray(sf::RenderWindow& window, const sf::VertexArray* vArray) const
    {
        window.draw(*vArray, m_transform);
    }

};


inline sf::Vector2f getMousePositionFloat(const sf::RenderWindow& window)
{
    sf::Vector2f mousePosition;
    mousePosition.x = sf::Mouse::getPosition(window).x / 10.f;
    mousePosition.y = sf::Mouse::getPosition(window).y / 10.f;

    return mousePosition;
}