#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <SFML/System/Vector2.hpp>

/* Body holds the physical properties of a cell: position, velocity, mass and radius.

   Layout (80 bytes, hot data first):
	 [ 0..31]  position, velocity, acceleration, inv_mass, radius  <- touched by every physics / wall pass
	 [32..43]  mass, impulse, id
	 [44..77]  nearby ids + count, active                          <- collision pass only

   mass_ is private so inv_mass_ can never go stale. Use set_mass(float) / get_mass(). */

using BodyId = uint32_t;

// Mass -> radius curve: a straight line through (mass_lo, radius_lo) and (mass_hi, radius_hi).
// It tracks the old power law (radius = scale * mass^0.817) to within ~2 units between the anchors,
// but costs one multiply-add instead of a std::pow.
namespace body_curve
{
	constexpr float mass_lo = 50.f;
	constexpr float mass_hi = 150.f;
	constexpr float radius_lo = 55.f;   // keep the radii in step with CellGeneticConstraints::radius
	constexpr float radius_hi = 135.f;
	constexpr float radius_min = 20.f;  // floor so light cells don't shrink to nothing: set to your smallest real cell radius

	static_assert(mass_lo > 0.f && mass_hi > mass_lo, "mass anchors must be positive and increasing");
	static_assert(radius_lo > 0.f && radius_hi > radius_lo, "radius anchors must be positive and increasing");

	constexpr float slope = (radius_hi - radius_lo) / (mass_hi - mass_lo); // 0.8
	constexpr float inv_slope = 1.f / slope;
}

class alignas(16) Body
{
private:
	// ---- hot block (32 B) ----
	sf::Vector2f position_{};
	sf::Vector2f velocity_{};
	sf::Vector2f acceleration_{};
	float inv_mass_ = 0.f; // always 1/mass_, or 0 when mass_ <= 0. Lets force/impulse use a multiply, no divide, no branch.
	float radius_ = 0.f;

	// ---- warm ----
	float mass_ = 0.f;

public:
	static constexpr uint8_t max_nearby = 8;

	float impulse_ = 0.f; // impulse applied to this body, used for collision damage

	// This id references the body itself
	BodyId id_ = 0;

	// ---- cold-ish: collision pass ----
	// Keeping track of nearby ids to resolve collisions with
	std::array<BodyId, max_nearby> nearby_ids_{};
	uint8_t nearby_ids_size_ = 0;

	bool active = true;

	Body(BodyId id = 0) : id_(id) {}

	static float mass_to_radius(float mass)
	{
		return mass > 0.f
			? std::max(body_curve::radius_min, body_curve::radius_lo + (mass - body_curve::mass_lo) * body_curve::slope)
			: 0.f; // also maps NaN to 0
	}

	static float radius_to_mass(float radius)
	{
		return radius > 0.f
			? std::max(0.f, body_curve::mass_lo + (radius - body_curve::radius_lo) * body_curve::inv_slope)
			: 0.f;
	}

	void set_radius(float r) { radius_ = r > 0.f ? r : 0.f; }
	void set_mass(float mass)
	{
		mass_ = mass > 0.f ? mass : 0.f; // also maps NaN to 0
		inv_mass_ = mass_ > 0.f ? 1.f / mass_ : 0.f;
	}
	void set_radius() { radius_ = mass_to_radius(mass_); } // radius_ from mass_
	void set_mass() { set_mass(radius_to_mass(radius_)); } // mass_ (and inv_mass_) from radius_, inverse of set_radius

	// For cells that grow every tick: updates mass and radius together without recomputing the curve.
	// Matches set_radius() on the linear part of the curve (above the radius_min floor).
	void add_mass(float dm)
	{
		set_mass(mass_ + dm);
		radius_ = mass_ > 0.f ? std::max(body_curve::radius_min, radius_ + dm * body_curve::slope) : 0.f;
	}

	[[nodiscard]] const sf::Vector2f& get_pos() const { return position_; }
	[[nodiscard]] const sf::Vector2f& get_vel() const { return velocity_; }
	[[nodiscard]] const sf::Vector2f& get_acc() const { return acceleration_; }
	[[nodiscard]] const float get_mass() const { return mass_; }
	[[nodiscard]] const float inv_mass() const { return inv_mass_; }
	[[nodiscard]] const	float get_radius() const { return radius_; }

	void translate(const sf::Vector2f& d) { position_ += d; }
	void set_pos(const sf::Vector2f& pos) { position_ = pos; }
	void set_vel(const sf::Vector2f& vel) { velocity_ = vel; }

	void apply_impulse(const sf::Vector2f& j) { velocity_ += j * inv_mass_; }
	void apply_force(const sf::Vector2f& force) { acceleration_ += force * inv_mass_; }

	// For accelerations that are already mass-independent.
	void apply_acceleration(const sf::Vector2f& a) { acceleration_ += a; }

	// Returns false (and drops the id) when the list is full.
	bool add_nearby(BodyId other)
	{
		if (nearby_ids_size_ >= max_nearby)
			return false;
		nearby_ids_[nearby_ids_size_++] = other;
		return true;
	}

	void update_physics()
	{
		velocity_ += acceleration_;
		position_ += velocity_;
		acceleration_ = { 0.f, 0.f };
	}

	void reset()
	{
		acceleration_ = { 0.f, 0.f };
		velocity_ = { 0.f, 0.f };
		nearby_ids_size_ = 0;
		impulse_ = 0.f;
		mass_ = 0.f;
		inv_mass_ = 0.f;
		radius_ = 0.f;
	}

	void copy(const Body* other)
	{
		position_ = other->position_;
		velocity_ = other->velocity_;
		acceleration_ = { 0.f, 0.f }; // don't inherit the parent's pending forces (and don't keep our own stale ones)
		mass_ = other->mass_;
		inv_mass_ = other->inv_mass_;
		radius_ = other->radius_;
		impulse_ = 0.f;               // a copy shouldn't inherit the parent's collision damage
		nearby_ids_size_ = 0;
	}

	// coeff: total drag coefficient for this tick (world + cell), quad: speed term
	// loss = coeff * radius / mass * (1 + quad * speed); velocity is scaled by (1 - loss), clamped at 0.
	void apply_drag(const float coeff, const float quad = 0.06f)
	{
		const float speed_sq = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
		if (speed_sq < 1e-12f || inv_mass_ <= 0.f)
			return;

		const float speed = std::sqrt(speed_sq);
		const float loss = coeff * radius_ * inv_mass_ * (1.f + quad * speed);
		velocity_ *= std::max(0.f, 1.f - loss);
	}
};

static_assert(sizeof(Body) <= 80, "Body grew past 80 bytes: check the layout comment at the top of the class");
static_assert(std::is_trivially_copyable_v<Body>, "Body must stay memcpy-able");