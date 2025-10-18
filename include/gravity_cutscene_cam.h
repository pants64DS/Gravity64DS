#pragma once
#include "SM64DS_PI.h"
#include "gravity_cam_ctrl.h"

class CutsceneCam : public CamCtrl
{
	Matrix4x3 transform;
	virtual bool CalculateTransform(Matrix4x3& res, Camera& cam, Player& player) final override;

	virtual bool CanChangeField(const Player& player, const GravityField& newField) const final override
	{
		return true;
	}

public:
	CutsceneCam(std::unique_ptr<CamCtrl> prev, const GravityField& field, Player& player):
		CamCtrl(std::move(prev), field, Type::CUTSCENE)
	{}
};
