#pragma once
#include "TMInterfaceImpl.h"
#include "SimulationController.h"
#include <math.h>
#include <algorithm>
#include <random>

enum SAPhase
{
	INITIAL,
	SEARCH
};

struct TriggerHit
{
	int time;
	float distance;

	TriggerHit(int time, float distance) :
		time(time), distance(distance)
	{

	}

	friend std::ostream& operator<<(std::ostream& os, const TriggerHit& hit)
	{
		os << "TriggerHit{" << hit.time << " (" << hit.distance << ")}";
		return os;
	}


	bool operator<(const TriggerHit& hit) const
	{
		if (time < hit.time) {
			return true;
		}

		if (time == hit.time && distance < hit.distance) {
			return true;
		}

		return false;
	}
};

class BFSearchController : public SimulationController
{
public:
	BFSearchController();

	void onSimulationBegin();
	void onSimulationEnd();
	bool onSimulationStep(const int time);
	void onAfterSimulationStep() {}

	void onGameInputBufferAvailable(GameEventBuffer& inputBuffer) {}

	void onCheckpointCountChanged();
private:
	SAPhase m_phase = INITIAL;
	MemSimulationState m_startState;
	std::vector<TMEvent> m_originalInputs;
	std::vector<TMEvent> m_currentInputs;
	std::vector<TMEvent> extendedStorageInputs;
	std::vector<int> m_currentCpTimes;
	std::vector<int> m_targetCpTimes;

	std::vector<Trigger3D> m_customTriggers;
	std::vector<TriggerHit> m_currentTriggerTimes;
	std::vector<TriggerHit> m_targetTriggerTimes;
	int m_currentTrigger = 0;

	int m_bestTime = -1;
	int m_iterations = 0;
	int m_modified = 0;
	int m_noFinishStreak = 0;
	bool m_simulating = false;
	byte m_steerIndex = 0;
	std::vector<std::vector<unsigned>> m_steerSequences;

	std::random_device rd = std::random_device{};
	std::default_random_engine rng = std::default_random_engine{ rd() };

	Vec3 m_prevPos;

	void reset();
	void startSearchPhase();
	void readInputsFromBuffer(std::vector<TMEvent>& inputs);
	void preventSimulationEnd(SimulationManager& simManager);
	void randomNeighbour();
	void populateSteerSequences();

	void extendSteerInputs();

	void printSaveInputs();
};

