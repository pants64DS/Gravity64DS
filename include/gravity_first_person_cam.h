#pragma once
#include "SM64DS_PI.h"
#include "gravity_cam_ctrl.h"

class FirstPersonCam : public CamCtrl
{
	s16 angX;
	s16 angY;
	s16 playerAngY;
	bool valid;
	Vector3 playerPos;
	Matrix3x3 gravityMatrix;

	virtual bool CalculateTransform(Matrix4x3& res, Camera& cam, Player& player) final override;
	virtual bool CanChangeField(const Player& player, const GravityField& newField) const final override;

public:
	FirstPersonCam(
		std::unique_ptr<CamCtrl> prev,
		const GravityField& field,
		Player& player
	):
		CamCtrl(std::move(prev), field, Type::FIRST_PERSON),
		angX(0),
		angY(player.ang.y - 180_deg),
		playerAngY(player.ang.y),
		valid(true),
		playerPos(player.pos),
		gravityMatrix(ActorExtension::Get(player).GetGravityMatrix())
	{}
};
