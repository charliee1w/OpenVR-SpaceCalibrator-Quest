#pragma once

#include "CalibrationCalc.h"
#include "Calibration.h"
#include <string>
#include <vector>

// One calibration mapping: reference tracking space -> target tracking space.
struct CalibrationChain {
	std::string name;
	std::string referenceTrackingSystem;
	std::string targetTrackingSystem;

	StandbyDevice referenceStandby;
	StandbyDevice targetStandby;

	Eigen::Vector3d calibratedRotation = Eigen::Vector3d::Zero();
	Eigen::Vector3d calibratedTranslation = Eigen::Vector3d::Zero();
	double calibratedScale = 1.0;

	Eigen::AffineCompact3d refToTargetPose = Eigen::AffineCompact3d::Identity();
	bool relativePosCalibrated = false;
	bool lockRelativePosition = false;
	bool autostartContinuous = false;
	bool valid = false;

	int32_t referenceID = -1;
	int32_t targetID = -1;
	bool slamReference = false;

	Eigen::Vector3d continuousCalibrationOffset = Eigen::Vector3d::Zero();
	bool continuousActive = false;

	CalibrationCalc calibration;
};

extern std::vector<CalibrationChain> CalChains;

void EnsureDefaultChain();
void SyncCalCtxToPrimaryChain();
void SyncPrimaryChainToCalCtx();
int FindChainIndexForTargetSystem(const std::string& trackingSystem);
bool AssignChainTargets(CalibrationChain& chain);
bool CollectChainSample(CalibrationChain& chain, const CalibrationContext& ctx);
void ProcessContinuousChain(CalibrationChain& chain, CalibrationContext& ctx, double time);
void StartContinuousChains();
void EndContinuousChains();