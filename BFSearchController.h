/* Copyright Adam Bieñkowski <donadigos159@gmail.com> - All Rights Reserved
 * Unauthorized redistribution and copying of this software and its source code via any medium is strictly prohibited.
 * Proprietary and confidential.
 */

#pragma once
#include "SimulationController.h"
#include "SimulationManager.h"
#include "TriggerManager.h"
#include <math.h>
#include <algorithm>
#include <random>
#include <fmt/format.h>

class BFSearchController : public SimulationController
{
public:
	BFSearchController();
	~BFSearchController() = default;

	void OnSimulationBegin(SimulationManager& simManager) override;
	void OnSimulationEnd(SimulationManager& simManager, const uint32_t result) override;
	void OnSimulationStep(SimulationManager& simManager, const int time) override;
	void OnCheckpointCountChanged(SimulationManager& simManager) override;
private:
	BFPhase m_phase = BFPhase::kInitial;
	BFTarget m_userTarget = BFTarget::kFinishTime;
	int m_userTargetTrigger = -1;
	int m_userTargetCheckpoint = -1;

	std::vector<TMEvent> m_originalInputs;
	std::vector<TMEvent> m_currentInputs;
	std::vector<int> m_currentCpTimes;
	std::vector<int> m_targetCpTimes;

	std::vector<TriggerHit> m_currentTriggerHits;
	std::vector<TriggerHit> m_targetTriggerHits;
	Trigger3D* m_currentTrigger = nullptr;

	int m_bestTime = -1;
	unsigned m_modified = 0;
	unsigned m_iterations = 0;
	unsigned m_improvements = 0;
	bool m_simulating = false;
	ControlNamesData m_controlNames;
	std::vector<std::vector<unsigned>> m_steerSequences;
	std::vector<MemSimulationState> m_origStates;

	bool m_settingBruteforce = false;
	bool m_settingPrintCps = false;
	bool m_settingInputsSteerFill = false;
	bool m_settingSearchForever = false;
	bool m_settingSimDebug = false;
	double m_settingModifyChance = 0;
	//int m_settingModifyNumber = 0;
	int m_settingMaxSteerDiff = 0;
	int m_settingMaxTimeDiff = 0;
	int m_settingOverrideStopTime = -1;
	int m_settingInputsMinTime = -1;
	int m_settingInputsMaxTime = -1;

	int m_eventsDuration = -1;

	std::random_device rd = std::random_device{};
	std::default_random_engine rng = std::default_random_engine{ rd() };

	Vec3 m_prevPos;

	void UpdateSettings();
	void EvaluateAll(SimulationManager& simManager, const int raceTime);
	void StartInitialPhase();
	void ResetWithNewSolution(SimulationManager& simManager);
	void StartNewIteration(SimulationManager& simManager, int rewindTime = -1, const bool printIteration = false);
	void StartSearchPhase(SimulationManager& simManager);
	void ReadInputsFromBuffer(SimulationManager& simManager, std::vector<TMEvent>& inputs);
	int RandomNeighbour(SimulationManager& simManager, const unsigned nCall = 0);
	bool ProcessTriggers(SimulationManager& manager, const int time, const Vec3& carPos, const Vec3& carVelocity, std::vector<TriggerHit>& hits);
	bool AskClientForEvaluation(SimulationManager& manager, const int time);
	void PrintSaveInputs(SimulationManager& simManager, const int score);
};

