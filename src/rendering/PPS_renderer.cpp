#include "PPS_renderer.h"
#include "particle_colors.h"

// ── Global color scheme (definition) ──────────────────────────────────────────
// Declared extern in particle_colors.h; defined here (render thread only).
ParticleColorScheme g_color_scheme{};
bool                g_color_scheme_dirty = true;

// ── Circle texture ────────────────────────────────────────────────────────────
inline sf::Texture generate_circle_texture(float radius)
{
    const auto r = static_cast<unsigned>(radius * 2.f);

    sf::RenderTexture renderTexture({ r, r });
    sf::CircleShape   circle(radius);

    circle.setFillColor(sf::Color::White);
    circle.setOrigin({ radius, radius });
    circle.setPosition({ radius, radius });
    renderTexture.clear(sf::Color::Transparent);
    renderTexture.draw(circle);
    renderTexture.display();

    return renderTexture.getTexture();
}

// ── Constructor ───────────────────────────────────────────────────────────────
PPS_Renderer::PPS_Renderer(sf::RenderWindow* window) : window_(window)
{
    constexpr int initial_vertex_count = ParticleSettings::initial_particle_count * 6;
    vertex_array.resize(initial_vertex_count);

    debug_lines_.setPrimitiveType(sf::PrimitiveType::Lines);

    texture = generate_circle_texture(ParticleSettings::particle_radius_max);
    texture.setSmooth(true);

    states.blendMode = sf::BlendAdd;

    heatmap.set_trail_decay(.45);

    std::cout << "Renderer Initialized\n";
}

// ── Zoom → alpha helper ───────────────────────────────────────────────────────
float PPS_Renderer::zoom_to_alpha(const float zoom,
    const float zoom_min, const float zoom_max,
    const float alpha_min, const float alpha_max)
{
    const float t = std::clamp((zoom - zoom_min) / (zoom_max - zoom_min), 0.f, 1.f);
    return alpha_min + t * (alpha_max - alpha_min);
}

// ── Main render ───────────────────────────────────────────────────────────────
void PPS_Renderer::render(const SimSnapshot& snapshot, Camera& camera)
{
    bool rend_particles = snapshot.toggles.render_particles;
    bool rend_map = true;

    const float left = camera.mapPixelToCoords({ 0, 0 }).x;
    const float right = camera.mapPixelToCoords(
        { static_cast<int>(screen_width_), 0 }).x;
    const float visible_world_width = right - left;

    const float transition_thresh_begin = 1500.f * ParticleSettings::particle_radius_min;
    const float transition_thresh_end = 1700.f * ParticleSettings::particle_radius_min;
    const float diff = transition_thresh_end - transition_thresh_begin;

    const auto& tgl = snapshot.toggles;

    float alpha_heat_map;
    float alpha_particles;

    if (tgl.auto_heatmap)
    {
        // Original zoom-based blending — both alphas derived from zoom level
        alpha_heat_map = std::clamp(
            (visible_world_width - transition_thresh_begin) / diff * 255.f,
            0.f, 255.f);
        alpha_particles = 255.f - alpha_heat_map;

        //if (rend_particles != false)
        rend_particles = visible_world_width < transition_thresh_end;

        rend_map = visible_world_width > transition_thresh_begin;
    }
    else
    {
        // Manual override — each mode is fully on or fully off, independently
        rend_map = tgl.force_heatmap;
        rend_particles = tgl.render_particles;

        alpha_heat_map = rend_map ? 255.f : 0.f;
        alpha_particles = rend_particles ? 255.f : 0.f;
    }

    if (rend_map)       render_heat_map(snapshot, camera, alpha_heat_map);
    if (rend_particles) render_particles(snapshot, camera, alpha_particles);
}


// ── Heat map ──────────────────────────────────────────────────────────────────
void PPS_Renderer::render_heat_map(const SimSnapshot& snapshot,
    const Camera& camera, const float alpha)
{
    // turning a vector of sf::Vector2fs into two x and y vectors
	auto* vec2f_vector = &snapshot.render.positions;


    heatmap.clear();
    heatmap.scatter2f(snapshot.render.positions, camera.m_view_);
    heatmap.upload();
    heatmap.draw(*window_, static_cast<uint8_t>(alpha));
}

// ── Particle rendering ────────────────────────────────────────────────────────
void PPS_Renderer::render_particles(const SimSnapshot& snapshot,
    const Camera& camera, const float alpha)
{
    const auto  tex_size = static_cast<float>(texture.getSize().x);
    const float u1 = tex_size, v1 = tex_size;

    const sf::Vector2f top_left = camera.mapPixelToCoords({ 0, 0 });
	sf::Vector2i screen_pos = { static_cast<int>(screen_width_), static_cast<int>(screen_height_) };
    const sf::Vector2f bottom_right = camera.mapPixelToCoords(screen_pos);


    vertex_array.clear();

    int particle_count = snapshot.stats.cell_particle_count;
    for (size_t i = 0; i < particle_count; ++i)
    {

        const sf::Vector2f pos = snapshot.render.positions[i];
        const float px_v = pos.x, py_v = pos.y;
        const float r = snapshot.render.radii[i];

        if (px_v < top_left.x || py_v < top_left.y ||
            px_v > bottom_right.x || py_v > bottom_right.y)
            continue;

        sf::Color col = snapshot.render.colors[i];
        col.a = alpha;

        vertex_array.append({ .position = { px_v - r, py_v - r }, .color = col, .texCoords = { 0.f, 0.f } });
        vertex_array.append({ .position = { px_v + r, py_v - r }, .color = col, .texCoords = { u1,  0.f } });
        vertex_array.append({ .position = { px_v + r, py_v + r }, .color = col, .texCoords = { u1,  v1  } });
        vertex_array.append({ .position = { px_v - r, py_v - r }, .color = col, .texCoords = { 0.f, 0.f } });
        vertex_array.append({ .position = { px_v + r, py_v + r }, .color = col, .texCoords = { u1,  v1  } });
        vertex_array.append({ .position = { px_v - r, py_v + r }, .color = col, .texCoords = { 0.f, v1  } });
    }

    states.texture = &texture;
    window_->draw(vertex_array, states);
}

// ── Debug overlay ─────────────────────────────────────────────────────────────
void PPS_Renderer::render_debug(const SimSnapshot& snapshot,
    const sf::Vector2f mouse_pos, const float mouse_radius)
{
 
}
